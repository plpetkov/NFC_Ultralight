readID : readID.o nfc.o wiringPiSPI.o wiringPi.o piHiPri.o piThread.o 
	@gcc readID.o nfc.o wiringPiSPI.o softTone.o softPwm.o wiringPi.o piHiPri.o piThread.o -o readID -lpthread
readID.o : readID.c nfc.h wiringPiSPI.h m_client.h
	@gcc -c readID.c -g 
nfc.o : nfc.c nfc.h wiringPiSPI.h
	@gcc -c nfc.c -g 
wiringPiSPI.o : wiringPiSPI.c wiringPiSPI.h
	@gcc -c wiringPiSPI.c -g 
softTone.o: wiringPi.h softTone.h
	@gcc -c softTone.c -g
softPwm.o: wiringPi.h softPwm.h
	@gcc -c softPwm.c -g
wiringPi.o : wiringPi.c wiringPi.h softPwm.h softTone.h
	@gcc -c wiringPi.c -g
piHiPri.o : piHiPri.c
	@gcc -c piHiPri.c -g
piThread.o : piThread.c
	@gcc -c piThread.c -g

clean :
	@rm *.o readID 
install : 
	@cc -shared -fpic -o libNFC.so nfc.c wiringPiSPI.c wiringPi.c piHiPri.c piThread.c -lpthread
	@sudo mv libNFC.so /usr/lib




