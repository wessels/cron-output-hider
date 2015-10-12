P=cron-output-hider

all:: ${P}

${P}: ${P}.o
	${CC} -o $@ ${P}.o

install:: ${P}
	install -m 755 ${P} /usr/local/sbin


clean::
	rm -f ${P}
	rm -f ${P}.o
