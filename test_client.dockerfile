FROM gcc:latest
RUN mkdir /app
COPY . /app
WORKDIR /app
RUN make test
RUN chmod +x bin/test
EXPOSE 25565-40000/udp 25565-40000/tcp
WORKDIR /app/bin
CMD ["./test", "client", "0.0.0.0", "25565"]