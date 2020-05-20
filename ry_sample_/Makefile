INCLUDE = -I. -Iinclude
LIBRARY =

CXXFLAGS    = -Wall -ggdb -pthread -D__DEBUG__
CXXFLAGS_R    = -Wall -pthread -O2 -O3 -fno-strict-aliasing

LINKFLAGS   = -Wl,-rpath,./:../bin extlibs/libevwork.a
LINKFLAGS_R = -Wl,-rpath,./:../bin extlibs/libevwork.ra

CXXFLAGS_EXTERN =
LINKFLAGS_EXTERN = extlibs/libtinyredis.ra -lhiredis -lev -ljsoncpp

TARGET   = ry_sample
TARGET_R   = ry_sample_r

SRC_FILES   = $(wildcard *.cpp base/*.cpp)

OBJ_FILES   = $(SRC_FILES:.cpp=.o)
OBJ_FILES_R   = $(SRC_FILES:.cpp=.ro)

$(TARGET) : $(OBJ_FILES)
	g++ -o $@ $(OBJ_FILES) $(CXXFLAGS) $(LINKFLAGS) $(CXXFLAGS_EXTERN) $(LINKFLAGS_EXTERN)
	
$(TARGET_R) : $(OBJ_FILES_R)
	g++ -o $@ $(OBJ_FILES_R) $(CXXFLAGS_R) $(LINKFLAGS_R) $(CXXFLAGS_EXTERN) $(LINKFLAGS_EXTERN)

%.o : %.cpp
	g++ -c -o $@ $< $(CXXFLAGS) $(CXXFLAGS_EXTERN) $(INCLUDE)
%.ro : %.cpp
	g++ -c -o $@ $< $(CXXFLAGS_R) $(CXXFLAGS_EXTERN) $(INCLUDE)

.PHONY : all clean

all : $(TARGET) $(TARGET_R)

clean :
	-rm $(OBJ_FILES) $(OBJ_FILES_R)
	-rm $(TARGET) $(TARGET_R)
