FROM alpine:3.21

RUN addgroup -S john  && adduser -S john -G john
WORKDIR /home/john
USER john
CMD ["/bin/sh"]