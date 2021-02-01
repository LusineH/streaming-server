# Stream Video using Gstreamer RTSP Server
We will stream the a video file using Gstreamer RTSP server, this streaming endpoint can be consumed by any RTSP client. 

### Setup RTSP server on Ubuntu 16:04
Use below are command to install Gstreamer and RTSP server along with its dependencies.
```bash
# Install gstreamer
sudo apt-get update
sudo apt-get install -y \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev wget git vim python3-pip libgstrtspserver-1.0-0 \
    libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-doc gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-pulseaudio libgstrtspserver-1.0-0
```
```bash
# Install rtsp
wget https://gstreamer.freedesktop.org/src/gst-rtsp-server/gst-rtsp-server-1.4.0.tar.xz
tar -xf gst-rtsp-server-1.4.0.tar.xz
cd gst-rtsp-server-1.4.0
./configure 
make
make check
make install
make installcheck
```

# Compile the code
gcc -o stream  stream.cpp  `pkg-config --cflags --libs gstreamer-rtsp-server-1.0`  
gcc -o receiver receiver.cpp  `pkg-config --cflags --libs gstreamer-1.0`

# Applications
There are 2 applications - streamer and receiver.
####Streamer
By default streaming application streams video stream to the following address:
```bash
 ./stream
Stream ready at rtsp://127.0.0.1:8554/frames
```

You can change the endpoint and the port, for example:
```bash
 ./stream -m /record -p 5000
Stream ready at rtsp://127.0.0.1:5000/record
```  

Please note that stream is protected by authentication:  
user - user  
password - password

#### Receiver
The stream is consumed by the receiver application:
```bash
 # The output directory should pre-exist.
 mkdir frames
 ./receiver -l rtsp://user:password@127.0.0.1:8554/frames
```
Based on the URL path, receiver performs different ways:  
In case of /frames it:
1. Processes and decodes the HLS stream
2. Encodes the frames to JPEG and saves in the ./frames directory. Please note the directory should pre-exist.  

In case of /record it:
1. Processes and decodes the HLS stream
2. Encodes the stream to MP4 video file and saves in the current directory with the name - out_video.mp4

