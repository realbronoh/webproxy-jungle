# cpfile: cpfile.o csapp.o
# 	gcc -o cpfile cpfile.o csapp.o
# cpfile.o : cpfile.c
# 	gcc -c cpfile.c
# csapp.o : csapp.c
# 	gcc -c csapp.c

# clean:
# 	rm -f *.o
# 	rm -f cpfile

TARGET = echoclient 
OBJS = echoclient.o csapp.o

%.o: %.c
	gcc -c $<
$(TARGET): $(OBJS)
	gcc -o $(TARGET) $(OBJS)

clean:
	rm -f *.o
	rm -f $(TARGET)