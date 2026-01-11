.PHONY: build install launch clean format lint

build:
	ufbt

install: build
	ufbt install

launch:
	ufbt launch

clean:
	ufbt clean

format:
	ufbt format

lint:
	ufbt lint
