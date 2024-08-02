FROM gcc:latest
RUN mkdir /app
COPY . /app
WORKDIR /app
RUN make
RUN chmod +x bin/sleep_server
EXPOSE 25565-40000/udp 25565-40000/tcp
WORKDIR /app/bin
CMD ["sleep", "infinity"]