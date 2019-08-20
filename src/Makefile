CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wimplicit-function-declaration -Wimplicit-int -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -c -O3
CXXFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -Wold-style-cast -c -O3
INCLUDES = 
LD = gcc
LDFLAGS = -shared -Wl,--dll,--enable-stdcall-fixup -static
LIBS = -ljpeg -lpng -lz
RM = rm -f

TARGET = image_output_ks.auo
OBJS = image_output_ks.o
RES = image_output_ks.res
DEF = image_output_ks.def

.SUFFIXES: .cpp .o
.SUFFIXES: .c .o
.SUFFIXES: .rc .res

all: $(TARGET)

$(TARGET): $(OBJS) $(RES) $(DEF)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(RES) $(DEF) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

.rc.res:
	windres --input-format=rc --output-format=coff -o $@ $<

image_output_ks.o: output.h image_output_ks.h
image_output_ks.res: image_output_ks.h

clean:
	$(RM) $(TARGET) $(OBJS) $(RES)

release:
	make all
	strip $(TARGET)
	cp -f $(TARGET) ../
	make clean
