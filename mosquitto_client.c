#include <stdio.h>
#include <mosquitto.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#define MAX_BUF 1024

struct mosquitto *mosq = NULL;

// Callback functions for subscribing, publishing and connecting to the mosquitto broker.
void my_message_callback(void *userdata, const struct mosquitto_message *message)
{

	if(message->payloadlen){
		printf("%s %s\n", message->topic, message->payload);
		
	}else{
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}

void my_connect_callback(void *userdata, int result)
{
	int i;
	if(!result){
		fprintf(stderr, "Connected\n");
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}

void my_publish_callback(void *userdata, uint16_t mid)
{
	
	printf("Data Published (mid: %d)", mid);
	printf("\n");
}



void my_subscribe_callback(void *userdata, uint16_t mid, int qos_count, const uint8_t *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

void my_log_callback(void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("%s\n", str);
}

void publish(char * topic,char * data, int size){
	mosquitto_loop(mosq, -1);
	mosquitto_publish(mosq,NULL,topic,size,data,0,true);
	
}

int main()
{
	//initialise the variables and the named pipe
	int fd;
	char *myfifo = "/tmp/myfifo";
	
	char buf[MAX_BUF];
	int i;

	fd=open(myfifo, O_RDONLY);
	close(fd);	
	
	//set the MQTT connection details
	char *host = "192.168.1.42";
	int port = 1883;
	int keepalive = 60;
	void * clean_session ;
	
	char* id="Table1";
	mosquitto_lib_init();
	mosq = mosquitto_new(id, clean_session);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}
	
	//link the callback functions to mosquitto
	mosquitto_message_callback_set(mosq, my_message_callback);
	//mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);
	mosquitto_connect_callback_set(mosq, NULL);
	if(mosquitto_connect(mosq, host, port, keepalive,false)){
		fprintf(stderr, "Unable to connect.\n");
		return 1;
	}
	int r;
	//set up pointers for string manipulation
	char * token;
	char * vote;
	char comb[50];
	while(1){

		memset(buf,0, MAX_BUF);
		//wait for a message to come down the pipe
		fd=open(myfifo, O_RDONLY);
		r=read(fd,buf,MAX_BUF);
		//reconnect just in case we timed out
		mosquitto_reconnect(mosq);
		if (r!=-1 || r!=0){
			//Split the message from the pipe and send it via MQTT onto different topics (message format: tableNum_musicVote_drinkChoice)
			token = strtok(buf,"_");
			printf("Read: %d nfc/usage: %s\n", strlen(token),token);
			mosquitto_loop(mosq, -1);	
			mosquitto_publish(mosq,NULL,"nfc/usage",strlen(token),token,0,true);
			vote = strtok(NULL,"_");
		
			printf("Read: %d nfc/vote: %s\n", strlen(vote),vote);
			mosquitto_publish(mosq,NULL,"nfc/vote",strlen(vote),vote,0,true);
			token = strtok(NULL,"_");
			printf("Read: %d nfc/drink: %s\n", strlen(token),token);
			mosquitto_publish(mosq,NULL,"nfc/drink",strlen(token),token,0,true);		

			strcpy(comb,token);
			strcat(comb,"_");
			strcat(comb,vote);			
			//combine the drink choice and the vote for logging purposes
			printf("nfc/combined: %s\n",comb);
			mosquitto_publish(mosq,NULL,"nfc/combined",strlen(comb),comb,0,true);
			
		}
		mosquitto_loop(mosq, -1);
		close(fd);
	}
	return 0;
}



