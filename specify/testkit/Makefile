pk=packet/packet.c

.phony all:
all:rawpack trans netpack launch
.phony:
rawpack:
	gcc -o rawpack rawpack.c $(pk)
trans:
	gcc -o trans translator.c $(pk)
netpack:
	gcc -o netpack netpack.c $(pk)
launch:
	gcc -o launch launchpack.c $(pk) -pthread
cleanall:
	rm rawpack trans netpack launch
