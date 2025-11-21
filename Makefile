# Makefile para cliente FTP 

CLTOBJ= FloresD-clienteFTP.o connectsock.o connectTCP.o passivesock.o passiveTCP.o errexit.o

all: FloresD-clienteFTP

FloresD-clienteFTP:	${CLTOBJ}
	cc -o FloresD-clienteFTP ${CLTOBJ}

clean:
	rm $(CLTOBJ) 
