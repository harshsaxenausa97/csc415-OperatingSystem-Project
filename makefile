CC = gcc
CFLAGS = -g
BUILDDIRECTORY = .buildfiles
OBJECTS = $(addprefix $(BUILDDIRECTORY)/, $(addsuffix .o, commands filesystem fsLow hashmap fsdriver3 terminal))

$(BUILDDIRECTORY)/%.o : %.c | $(BUILDDIRECTORY)
	$(CC) $(CFLAGS) -c -o $@ $<

fsdriver3 : $(OBJECTS) 
	$(CC) $(CFLAGS) -o fsdriver3 $(OBJECTS) -lm -lreadline

$(BUILDDIRECTORY) :
	mkdir $(BUILDDIRECTORY)

$(BUILDDIRECTORY)/commands.o : commands.h hashmap.h
$(BUILDDIRECTORY)/filesystem.o : filesystem.h fsLow.h systemstructs.h
$(BUILDDIRECTORY)/fsdriver3.o : filesystem.h terminal.h
$(BUILDDIRECTORY)/fsLow.o : fsLow.h
$(BUILDDIRECTORY)/hashmap.o : hashmap.h
$(BUILDDIRECTORY)/terminal.o : terminal.h commands.h filesystem.h

clean :
	rm -r $(BUILDDIRECTORY)
	rm fsdriver3