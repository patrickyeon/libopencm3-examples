.PHONY: stats
##
## This file is part of the libopencm3 project.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

BINARY = cdcacm

LDSCRIPT = ../stm32f0-pat.ld

include ../../Makefile.include

stats: $(BINARY).elf
	$(Q)$(OBJDUMP) -S $(BINARY).elf >$(BINARY).lst
	$(Q)$(OBJDUMP) -t $(BINARY).elf >$(BINARY).map
	$(Q)grep ".\+" $(BINARY).map | sed 's/.hidden//' | awk '{print $$(NF - 1) " " $$NF}' | sort -r >$(BINARY).topmap
	$(Q)$(PREFIX)-size -x $(BINARY).elf
