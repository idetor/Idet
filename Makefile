CXX = g++
CXXFLAGS = -std=c++20
LDFLAGS = -lncursesw -lcurl
TARGET = idet
SRC = main.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET)
