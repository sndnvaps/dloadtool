TARGET = dloadtool

OBJ = util.o \
      dload.o \
      main.o

CFLAGS = -g
LDFLAGS =

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) *.o
