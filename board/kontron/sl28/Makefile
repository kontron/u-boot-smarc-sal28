#
# Copyright 2018 Kontron Europe GmbH
#
# SPDX-License-Identifier:	GPL-2.0+
#


ifndef CONFIG_SPL_BUILD
obj-y += sl28.o cmds.o psci.o
endif

obj-y += common.o ddr.o
obj-$(CONFIG_SPL_BUILD) += spl.o
