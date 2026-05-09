FROM debian:12-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y \
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
    ca-certificates \
    curl \
    g++ \
    && rm -rf /var/lib/apt/lists/*

RUN echo 'export PS1="\u@\h \w \$ "' >> /etc/bash.bashrc && \
    echo "alias about='cat README'" >> /etc/bash.bashrc && \
    echo "en_GB.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen en_GB.UTF-8 && \
    useradd -m guest && chmod -R 777 /home

USER guest
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --no-modify-path --profile minimal
ENV PATH="/home/guest/.cargo/bin:${PATH}"

ENV LANG=en_GB.UTF-8
ENV LANGUAGE=en_GB:en
ENV LC_ALL=en_GB.UTF-8