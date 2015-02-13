all:
	$(MAKE) -C iojs
	$(MAKE) -C iojsp
	$(MAKE) -C nginx


clean:
	$(MAKE) -C iojs clean
#	$(MAKE) -C iojsp clean
	$(MAKE) -C nginx clean


install:
	$(MAKE) -C iojs install
#	$(MAKE) -C iojsp install
	$(MAKE) -C nginx install
