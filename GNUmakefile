# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

VERSION := 19.08

ifneq ($(RTE_SDK),)
# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += src

include $(RTE_SDK)/mk/rte.extsubdir.mk
endif

.PHONY: showversion
showversion:
	@echo $(VERSION)

# Compile RST documents
DOC_ROOT = docs/guides

.PHONY: dist-clean
dist-clean:
	make clean
	rm -rf $(wildcard src/*/$(RTE_TARGET))
	rm -rf $(wildcard src/*/*.pyc)
	rm -rf $(wildcard src/*/__pycache__)
	rm -rf $(wildcard src/*/shared)
	rm -rf $(wildcard src/mirror/vf)
	rm -rf $(wildcard src/pcap/vf)

.PHONY: doc
doc: doc-all
doc-all: doc-pdf doc-html

.PHONY: doc-html
doc-html:
	make -C $(DOC_ROOT) html

RTE_PDF_DPI := 300
IMG_DIR := docs/guides/images
SVGS := $(wildcard $(IMG_DIR)/*.svg)
SVGS += $(wildcard $(IMG_DIR)/*/*.svg)
SVGS += $(wildcard $(IMG_DIR)/*/*/*.svg)
PDFS := $(SVGS:%.svg=%.pdf)

%.pdf: %.svg
	inkscape -d $(RTE_PDF_DPI) -D -f $< -A $@ $(RTE_INKSCAPE_VERBOSE)

.PHONY: doc-pdf
doc-pdf: $(PDFS)
	make -C $(DOC_ROOT) latexpdf
	@echo "Succeeded to generate '$(DOC_ROOT)/_build/latex/SoftPatchPanel.pdf'"

.PHONY: doc-clean
doc-clean:
	find $(DOC_ROOT)/images/ -type f -name "*.pdf" -delete
	make -C $(DOC_ROOT) clean
