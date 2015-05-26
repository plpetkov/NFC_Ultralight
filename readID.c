#include "nfc.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#define ULTRALIGHT_READ_SIZE 4 // we should be able to read 16 bytes at a time
#define ULTRALIGHT_PAGE_SIZE 4
#define ULTRALIGHT_DATA_START_PAGE 4
#define ULTRALIGHT_MESSAGE_LENGTH_INDEX 1
#define ULTRALIGHT_DATA_START_INDEX 2
#define ULTRALIGHT_MAX_PAGE 63

#define NFC_FORUM_TAG_TYPE_2 ("NFC Forum Type 2")



//#define  	NFC_DEMO_DEBUG 	1

#define TABLE_CHOICE table1
#define BAN_LENGTH 10
#define MAX_BUF 100
#define NEW_VOTE_TIME 60

unsigned int _cs=11;
unsigned int rst=2;    
unsigned int tagCapacity;
unsigned int messageLength;
unsigned int bufferSize;
unsigned int ndefStartIndex;

int flag;

int i;
int fd;
char * myfifo = "/tmp/myfifo";
char pipeBuf[100];
int vote_ban[BAN_LENGTH];
int ring=0;
char * vote;


//Create a ring buffer which holds the the IDs of the NFC tags that have already voted for a music choice.
//This is done to avoid spamming votes.
//The buffer is cleared after some time has passed.
//
boolean not_ban(int try){

	// print the buffer for debug purposes 
	int i;
	printf("Buff ");
	for (i=0;i<BAN_LENGTH;i++){
		printf("%d ",vote_ban[i]);
	}
	printf("\n");
	
	for (i=0;i<BAN_LENGTH;i++){
		if(try==vote_ban[i]) {
			return false;
		}
	}
	if (ring>BAN_LENGTH){
		ring=0;
	}
	vote_ban[ring]=try;
	ring++;
	return true;
}


//Check if the NFC tag is unformatted
boolean isUnformatted(){
	uint8_t page = 4;
    byte data[ULTRALIGHT_READ_SIZE];
    boolean success = ultralight_ReadPage (page, data);
    if (success)
    {
        return (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF);
    }
    else
    {
        printf("Error. Failed read page ");
		printf("%d\n",page);
        return false;
    }
}
//Find the tag capacity for storing messages
void readCapabilityContainer(){
	 byte data[ULTRALIGHT_PAGE_SIZE];
    int success = ultralight_ReadPage (3, data);
    if (success)
    {
        // See AN1303 - different rules for Mifare Family byte2 = (additional data + 48)/8
        tagCapacity = data[2] * 8;
	#ifdef NFC_DEMO_DEBUG
        printf("Tag capacity ");printf("%d",tagCapacity);printf(" bytes\n");
	#endif

        // TODO future versions should get lock information
    }              
	
}

//Calculate the buffer size depending on the type of NFC tag
void calculateBufferSize()
{
    // TLV terminator 0xFE is 1 byte
    bufferSize = messageLength + ndefStartIndex + 1;

    if (bufferSize % ULTRALIGHT_READ_SIZE != 0)
    {
        // buffer must be an increment of page size
        bufferSize = ((bufferSize / ULTRALIGHT_READ_SIZE) + 1) * ULTRALIGHT_READ_SIZE;
    }
} 

/* 
Find the start byte and the length of the main NFC tag message 
*/
void findNdefMessage()
{
    int page;
    byte data[12]; // 3 pages
    byte* data_ptr = &data[0];
    // the nxp read command reads 4 pages, unfortunately one page at a time
    boolean success = true;
    for (page = 4; page < 6; page++)
    {
        success = success && ultralight_ReadPage(page, data_ptr);
        //printf("Page ");printf("%d",page);printf(" - ");
        //PrintHexChar(data_ptr, 4);
        data_ptr += ULTRALIGHT_PAGE_SIZE;
    }

    if (success)
    {
        if (data[0] == 0x03)
        {
            messageLength = data[1];
            ndefStartIndex = 2;
        }
        else if (data[5] == 0x3) // page 5 byte 1
        {
            // TODO should really read the lock control TLV to ensure byte[5] is correct
            messageLength = data[6];
            ndefStartIndex = 7;
        }
    }
    #ifdef NFC_DEMO_DEBUG
    printf("messageLength ");printf("%d\n",messageLength);
    printf("ndefStartIndex ");printf("%d\n",ndefStartIndex);
    #endif	
}

// Extract the NFC tag message and send it through the pipe
void ndefMessage(const byte * data, const int numBytes){
 
    int recordCount = 0;

    int index = 0;
    int i;
    char *c;
	
    while (index <= numBytes)
    {

        // decode tnf - first byte is tnf with bit flags
        // see the NFDEF spec for more info
	byte tnf_byte = data[index];
        boolean mb = (tnf_byte & 0x80) != 0;
        boolean me = (tnf_byte & 0x40) != 0;
        boolean cf = (tnf_byte & 0x20) != 0;
        boolean sr = (tnf_byte & 0x10) != 0;
        boolean il = (tnf_byte & 0x8) != 0;
        byte tnf = (tnf_byte & 0x7);

        //NdefRecord record = NdefRecord();
        //record.setTnf(tnf);

        index++;
        int typeLength = data[index];

        int payloadLength = 0;
        if (sr)
        {
            index++;
            payloadLength = data[index];
        }
        else
        {
            payloadLength =
		((0xFF & data[++index]) << 24)
		| ((0xFF & data[++index]) << 16)
		| ((0xFF & data[++index]) << 8)
		| (0xFF & data[++index]);
        }

        int idLength = 0;
        if (il)
        {
            index++;
            idLength = data[index];
        }

        index++;

        //record.setType(&data[index], typeLength);
        index += typeLength;

        if (il)
        {
		printf("ID: ");
		for(i=index;i<idLength;i++)
		{
			printf("%d ",&data[i]);
		}
		printf("\n");
       
            index += idLength;
        }index += idLength;
	int cx;
	char *Temp_pay,*token;
	
	//start building the pipe message. 
	//add the table ID
	strcpy(pipeBuf,"table1_"); //remove hardcoded values
	//add the music choice (based on the NFC board selected)
	strcat(pipeBuf,vote);
	//extract the payload of the tag message message will be of the type : drink_ID ( where drink is the preprogrammed drink choice
	//  and	ID is the preprogrammed tag ID)
	printf("Payload: ");
	int k=0;
	for(i=index;i<payloadLength+index;i++)
	{
		c=(char*)&data[i];
		printf("%c",c[0]);
		if (k==1){
			//add it to the pipe message
			strcat(pipeBuf,c);
			//store it for some security checks
			Temp_pay=c;
		}
		k++;
		//snprintf(pipeBuf+cx,100-cx,"%c",c[0]);
	}
	printf("\n");
	//split the drink and ID, so that the ID can be checked against the Ban List
	strtok(Temp_pay,"_");
	token = strtok(NULL,"_");
	k=atoi(token);

	pipeBuf[strlen(pipeBuf)-1]=0;
	//check if the tag has not been ban for voting
	if (not_ban(k)){
		//Flash the LED to signal that data will come through
		digitalWrite(23,1);
		//Write to the pipe and close it
		open(myfifo, O_WRONLY);
		write(fd,pipeBuf,strlen(pipeBuf));
		printf("Pipe Write %s\n",pipeBuf);
		close(fd);
	}else {
		printf("ID banned for vote");
	}
        index += payloadLength;
      

        if (me) break; // last message
    }

	
}



void main(void) 
{

	
	uint32_t versiondata; 
	uint32_t iD;
	uint8_t uidLength;
	int r;
#ifdef NFC_DEMO_DEBUG
	printf("Hello!\n");
#endif

	//Initialise one of the NFC boards
	begin();
	versiondata = getFirmwareVersion();
	if (! versiondata) 
	{
		#ifdef NFC_DEMO_DEBUG
			printf("Didn't find PN53x board\n");
		#endif
		while (1); // halt
	}
	#ifdef NFC_DEMO_DEBUG
	// Got ok data, print it out!
	printf("Found chip PN5"); 
	printf("%x\n",((versiondata>>24) & 0xFF));
	printf("Firmware ver. "); 
	printf("%d",((versiondata>>16) & 0xFF));
	printf("."); 
	printf("%d\n",((versiondata>>8) & 0xFF));
	printf("Supports "); 
	printf("%x\n",(versiondata & 0xFF));
	#endif
	// configure board to read RFID tags and cards
	SAMConfig();
	

	//Once the initialisation is successful, change the chip select and reset pins so that we can initialise the other NFC board.
	_cs=10;
	rst=0;

	begin();
	versiondata = getFirmwareVersion();
	if (! versiondata) 
	{
		#ifdef NFC_DEMO_DEBUG
			printf("Didn't find PN53x board\n");
		#endif
		while (1); // halt
	}
	#ifdef NFC_DEMO_DEBUG
	// Got ok data, print it out!
	printf("Found chip PN5"); 
	printf("%x\n",((versiondata>>24) & 0xFF));
	printf("Firmware ver. "); 
	printf("%d",((versiondata>>16) & 0xFF));
	printf("."); 
	printf("%d\n",((versiondata>>8) & 0xFF));
	printf("Supports "); 
	printf("%x\n",(versiondata & 0xFF));
	#endif
	// configure board to read RFID tags and cards
	SAMConfig(); 

	//Create and open the named pipe
	mkfifo(myfifo,0666);
	fd = open(myfifo, O_WRONLY);
	printf ("FD: %d \n",fd);
	//Initialise and start the timer. Future improvements to be made
	time_t start,end;
	double diff;
	time(&start);
	//Configure two GPIO pins to be outputs. They'll be used for LED status lights 
	pinMode(25, OUTPUT);
	pinMode(23,OUTPUT);
	digitalWrite(25,1);
	
	while(1) 
	{
		//If enough time has passed a NFC tag is allowed to vote for music again. Clear the ID ban list
		time(&end);
		diff=difftime(end,start);
		
		digitalWrite(23,0);
		
		if (diff>NEW_VOTE_TIME){
			printf("It's time");
			for (i=0;i<BAN_LENGTH;i++){
				vote_ban[i]=0;
			}
			start=end;
		}
		// look for MiFare type cards
		uidLength=0;
		flag=0;
		iD=0;

		// Switch between the two NFC's and poll if a tag has been detected. Change the music choice depending on the NFC
		if(_cs==10){
			_cs=11;
			vote="rock_";
		}
		else {
			_cs=10;
			vote="classic_";
		}
		if(rst==0)
			rst=2;
		else rst=0;
		//depending on the Id Length we can distinguish between the two types of NFC tags
		iD = readPassiveTargetID(PN532_MIFARE_ISO14443A,(uint8_t*)&uidLength);
		if (uidLength==4){
			printf("MIFARE_CLASSIC\n");
		}	else  {
				if(uidLength!=0)
	 			{
				 printf("MIFARE_ULTRALIGHT\n");
				}
			}	
		if (iD != 0) 
		{
			//Clear the buffer holding the pipe message
			memset(pipeBuf,0,100);
			
			 if (isUnformatted())
			{
				printf("WARNING: Tag is not formatted.\n");
				return ;
			}
			readCapabilityContainer(); // meta info for tag
			findNdefMessage(); //find the programmed message on the tag
			calculateBufferSize(); //calculate the buffer size

			// data is 0x44 0x03 0x00 0xFE
			if(messageLength==0){
				printf("empty message\n");
				flag=1;
				//break;
			}
			boolean success;
			uint8_t page;
			uint8_t index = 0;
			byte buffer[bufferSize];
			// Read the full NFC ultralight tag for debugging purposes
			for (page = ULTRALIGHT_DATA_START_PAGE; page < ULTRALIGHT_MAX_PAGE; page++)
			{
				// read the data
				success = ultralight_ReadPage(page, &buffer[index]);
				if (success)
				{
					#ifdef NFC_DEMO_DEBUG
						printf("Page ");printf("%d",page);printf(" \n");
						PrintHexChar(&buffer[index], ULTRALIGHT_PAGE_SIZE);
					#endif
				}
				else
				{
					printf("Read failed ");printf("%d \n",page);
					// TODO error handling
					messageLength = 0;
					flag=1;
					break;
				}

				if (index >= (messageLength + ndefStartIndex))
				{	
					break;
				}

				index += ULTRALIGHT_PAGE_SIZE;
			}
			if(flag==0){
				ndefMessage(&buffer[ndefStartIndex], messageLength);
			}
		
			printf("Read card #%d\n",iD); 
		
		}
	}
}



   


