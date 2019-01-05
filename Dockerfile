FROM debian:stable-slim

ENV APT_GET_UPDATE 2019-01-01

WORKDIR /app
COPY . /app

RUN chmod +x toolchain/setup.sh
RUN ./toolchain/setup.sh

CMD [ "/bin/bash" ]