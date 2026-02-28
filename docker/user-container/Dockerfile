FROM debian:oldoldstable-20260223-slim
# RUN echo 'alias vim="TERM=vt100 vim"' >> /etc/bash.bashrc
RUN apt-get update && apt-get install -y \
    gcc \
    vim \
    neofetch \
    make \ 
    && rm -rf /var/lib/apt/lists/*