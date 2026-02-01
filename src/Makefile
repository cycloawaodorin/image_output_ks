CXX = g++
CXXWFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wpointer-arith -Wwrite-strings -Wmissing-noreturn -Wno-unused-parameter -Wsuggest-attribute=format -Wsuggest-attribute=noreturn -Wunused-variable -Winit-self -Wshadow -Wlogical-op -Wconversion -Wold-style-cast
CXXFLAGS = -c -O3 -std=c++23
INCLUDES = 
LD = g++
LDFLAGS = -shared -Wl,--dll,--enable-stdcall-fixup -static
LIBS = -static -lstdc++ -ljpeg -lpng -lz
RM = rm -f

TARGET = image_output_ks.auo
OBJS = image_output_ks.o
RES = image_output_ks.res
DEF = image_output_ks.def

.SUFFIXES: .cpp .o
.SUFFIXES: .rc .res

all: $(TARGET)

$(TARGET): $(OBJS) $(RES) $(DEF)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(RES) $(DEF) $(LIBS)

.cpp.o:
	$(CXX) $(CXXWFLAGS) $(CXXFLAGS) $(INCLUDES) -o $@ $<

.rc.res:
	windres --input-format=rc --output-format=coff -o $@ $<

image_output_ks.o: output.hpp image_output_ks.h
image_output_ks.res: image_output_ks.h

clean:
	$(RM) $(TARGET) $(OBJS) $(RES)

release:
	make all
	strip $(TARGET)
	cp -f $(TARGET) ../
	make clean
