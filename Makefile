default:
	gcc -o lab1a -std=c99 -g -Wall -Wextra lab1a.c
clean:
	rm -f lab1a lab1a-604902470.tar.gz
dist:
	tar -cvzf lab1a-604902470.tar.gz lab1a.c Makefile README
