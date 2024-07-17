FROM gcc:latest
RUN mkdir /app
COPY . /app
WORKDIR /app
RUN bash build.sh
RUN chmod +x bin/sleep_server
EXPOSE 25565-40000/udp
WORKDIR /app/bin
CMD ["./sleep_server", "manager"]