TARGET = dloadtool

OBJ = util.o \
      dload.o \
      main.o

CFLAGS = -g
LDFLAGS = -lcintelhex

all: $(TARGET)

debug: CFLAGS += -g
debug: all

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET) *.o
