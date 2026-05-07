CXX = g++
CXXFLAGS = -std=c++17 -Wall
LDFLAGS = -lsqlite3

SRC = src/main.cpp src/database.cpp src/statistics.cpp
OBJ = $(SRC:.cpp=.o)
EXEC = program.exe

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(EXEC)