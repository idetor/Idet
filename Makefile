CXX = g++
CXXFLAGS = -std=c++20
LDFLAGS = -lncursesw -lcurl
TARGET = editor
SRC = main.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET)
