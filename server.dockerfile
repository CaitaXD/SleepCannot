FROM gcc:latest
RUN mkdir /app
COPY . /app
WORKDIR /app
RUN apt update && apt install -y valgrind
RUN make
RUN chmod +x bin/sleep_server
EXPOSE 25565-40000/udp 25565-40000/tcp
WORKDIR /app/bin
CMD ["valgrind", "--track-origins=yes", "./sleep_server", "manager"]
#CMD ["sleep", "infinity"]