install:
	gcc -o my_beacon_flood main.c -lpcap

clean:
	rm -f my_beacon_flood