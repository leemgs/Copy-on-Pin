FROM ubuntu:20.04

WORKDIR /root/image

ADD . /root/image

# avoid getting stuck on tzdata prompt
ENV TZ=Europe/Kiev
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN echo 'sudo() { "$@"; }' >>~/bash_env.sh
ENV BASH_ENV=~/bash_env.sh

RUN bash ./provision.sh
