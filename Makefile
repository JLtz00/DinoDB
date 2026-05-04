CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SOURCES = src/main.cpp \
          src/storage/page.cpp \
          src/storage/wal.cpp \
          src/storage/disk_manager.cpp

mydb: $(SOURCES)
	$(CXX) $(CXXFLAGS) -I src $(SOURCES) -o mydb

clean:
	rm -f mydb *.db *.wal
