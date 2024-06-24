TARGET=DemoCannon
CC=g++
CFLAGS= -I../Common \
	-I../ssd1306_linux \
	-I../TfLite-2.17/include \
	-I../Servo \
	-I../TfLite-2.17/C-Imp \
	-I/usr/include/opencv4/ \
	-I/usr/include/libcamera \
	-DSERVER \
	-Wall

LDFLAGS=-lsqlite3 \
	-lssl \
	-lcrypto \
	-ltensorflowlite_c \
	-llgpio \
	-lopencv_core \
	-lopencv_highgui \
	-lopencv_imgcodecs \
	-lopencv_imgproc \
	-lopencv_video \
	-lopencv_videoio \
	-llccv \

SRCS=DemoCannon.cpp \
	../Database/class_database.cpp \
	../Common/CvImageMatch.cpp \
	../Servo/ServoPi.cpp \
	../Common/NetworkTCP.cpp \
	../Common/TcpSendRecvJpeg.cpp \
	../Common/KeyboardSetup.cpp \
	../Common/ObjectDetector.cpp \
	../ssd1306_linux/ssd1306.cpp \
	../ssd1306_linux/linux_i2c.cpp \
	../Common/AuditLog.cpp \
	
OBJS=$(SRCS:.cpp=.o)

.PHONY: all
all: $(TARGET)


.PHONY: clean
clean:
	rm $(OBJS) $(TARGET) images/*.jpg

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.cpp.o:
	$(CC) -c $< -o $@ $(CFLAGS)
