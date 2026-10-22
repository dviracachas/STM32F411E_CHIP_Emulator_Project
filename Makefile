PROJECTS := $(wildcard projects/*/)

all:
	for p in $(PROJECTS); do $(MAKE) -C $$p || exit 1; done

format:
	git ls-files 'projects/*.c' 'projects/*.h' | xargs -r clang-format -i

check-format:
	git ls-files 'projects/*.c' 'projects/*.h' | xargs -r clang-format --dry-run --Werror

.PHONY: all format check-format
