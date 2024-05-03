libscale.so: scale.o verbs.o
	gcc -shared -o libscale.so scale.o verbs.o
scale.o: scale.c scale.h log.h scale_type.h
	gcc -c -fPIC scale.c -o scale.o
verbs.o: verbs.c verbs.h scale.c scale.h log.h scale_type.h
	gcc -c -fPIC verbs.c -o verbs.o
clean:
	rm libscale.so *.o