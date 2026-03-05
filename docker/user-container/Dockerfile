FROM debian:12-slim
RUN apt-get update && apt-get install -y \
    gcc \
    vim \
    neofetch \
    make \
    htop \
    btop \
    locales \
    nano \
    binutils \
    python3 \
    g++ \
    && rm -rf /var/lib/apt/lists/*

RUN chmod -R 777 /home

RUN echo 'export PS1="\u@\h \w \$ "' >> /etc/bash.bashrc

RUN echo "en_GB.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen en_GB.UTF-8

RUN useradd -m guest && chown -R guest:guest /home

ENV LANG=en_GB.UTF-8
ENV LANGUAGE=en_GB:en
ENV LC_ALL=en_GB.UTF-8