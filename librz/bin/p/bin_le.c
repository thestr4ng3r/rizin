// SPDX-FileCopyrightText: 2019 GustavoLCR <gugulcr@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_bin.h>
#include "../format/le/le.h"

static bool check_buffer(RzBuffer *b) {
	ut64 length = rz_buf_size(b);
	if (length < 2) {
		return false;
	}

	ut16 idx;
	if (!rz_buf_read_le16_at(b, 0x3c, &idx)) {
		return false;
	}

	if ((ut64)idx + 26 < length) {
		ut8 buf[2];
		rz_buf_read_at(b, 0, buf, sizeof(buf));
		if (!memcmp(buf, "LX", 2) || !memcmp(buf, "LE", 2)) {
			return true;
		}
		if (!memcmp(buf, "MZ", 2)) {
			rz_buf_read_at(b, idx, buf, sizeof(buf));
			if (!memcmp(buf, "LX", 2) || !memcmp(buf, "LE", 2)) {
				return true;
			}
		}
	}
	return false;
}

static bool load_buffer(RzBinFile *bf, RzBinObject *obj, RzBuffer *buf, Sdb *sdb) {
	rz_return_val_if_fail(bf && obj && buf, false);
	rz_bin_le_obj_t *res = rz_bin_le_new_buf(buf);
	if (res) {
		obj->bin_obj = res;
		return true;
	}
	return false;
}

static void destroy(RzBinFile *bf) {
	rz_bin_le_free(bf->o->bin_obj);
}

static void header(RzBinFile *bf) {
	rz_return_if_fail(bf && bf->rbin && bf->o && bf->o->bin_obj);
	RzBin *rbin = bf->rbin;
	rz_bin_le_obj_t *bin = bf->o->bin_obj;
	LE_image_header *h = bin->header;
	PrintfCallback p = rbin->cb_printf;
	if (!h || !p) {
		return;
	}
	p("Signature: %2s\n", h->magic);
	p("Byte Order: %s\n", h->border ? "Big" : "Little");
	p("Word Order: %s\n", h->worder ? "Big" : "Little");
	p("Format Level: %u\n", h->level);
	p("CPU: %s\n", bin->cpu);
	p("OS: %s\n", bin->os);
	p("Version: %u\n", h->ver);
	p("Flags: 0x%04x\n", h->mflags);
	p("Pages: %u\n", h->mpages);
	p("InitialEipObj: %u\n", h->startobj);
	p("InitialEip: 0x%04x\n", h->eip);
	p("InitialStackObj: %u\n", h->stackobj);
	p("InitialEsp: 0x%04x\n", h->esp);
	p("Page Size: 0x%04x\n", h->pagesize);
	if (bin->is_le) {
		p("Last Page Size: 0x%04x\n", h->pageshift);
	} else {
		p("Page Shift: 0x%04x\n", h->pageshift);
	}
	p("Fixup Size: 0x%04x\n", h->fixupsize);
	p("Fixup Checksum: 0x%04x\n", h->fixupsum);
	p("Loader Size: 0x%04x\n", h->ldrsize);
	p("Loader Checksum: 0x%04x\n", h->ldrsum);
	p("Obj Table: 0x%04x\n", h->objtab);
	p("Obj Count: %u\n", h->objcnt);
	p("Obj Page Map: 0x%04x\n", h->objmap);
	p("Obj Iter Data Map: 0x%04x\n", h->itermap);
	p("Resource Table: 0x%04x\n", h->rsrctab);
	p("Resource Count: %u\n", h->rsrccnt);
	p("Resident Name Table: 0x%04x\n", h->restab);
	p("Entry Table: 0x%04x\n", h->enttab);
	p("Directives Table: 0x%04x\n", h->dirtab);
	p("Directives Count: %u\n", h->dircnt);
	p("Fixup Page Table: 0x%04x\n", h->fpagetab);
	p("Fixup Record Table: 0x%04x\n", h->frectab);
	p("Import Module Name Table: 0x%04x\n", h->impmod);
	p("Import Module Name Count: %u\n", h->impmodcnt);
	p("Import Procedure Name Table: 0x%04x\n", h->impproc);
	p("Per-Page Checksum Table: 0x%04x\n", h->pagesum);
	p("Enumerated Data Pages: 0x%04x\n", h->datapage);
	p("Number of preload pages: %u\n", h->preload);
	p("Non-resident Names Table: 0x%04x\n", h->nrestab);
	p("Size Non-resident Names: %u\n", h->cbnrestab);
	p("Checksum Non-resident Names: 0x%04x\n", h->nressum);
	p("Autodata Obj: %u\n", h->autodata);
	p("Debug Info: 0x%04x\n", h->debuginfo);
	p("Debug Length: 0x%04x\n", h->debuglen);
	p("Preload pages: %u\n", h->instpreload);
	p("Demand pages: %u\n", h->instdemand);
	p("Heap Size: 0x%04x\n", h->heapsize);
	p("Stack Size: 0x%04x\n", h->stacksize);
}

static RzList /*<RzBinSection *>*/ *sections(RzBinFile *bf) {
	return rz_bin_le_get_sections(bf->o->bin_obj);
}

static RzList /*<RzBinAddr *>*/ *entries(RzBinFile *bf) {
	return rz_bin_le_get_entrypoints(bf->o->bin_obj);
}

static RzList /*<RzBinSymbol *>*/ *symbols(RzBinFile *bf) {
	return rz_bin_le_get_symbols(bf->o->bin_obj);
}

static RzList /*<RzBinImport *>*/ *imports(RzBinFile *bf) {
	return rz_bin_le_get_imports(bf->o->bin_obj);
}

static RzList /*<char *>*/ *libs(RzBinFile *bf) {
	return rz_bin_le_get_libs(bf->o->bin_obj);
}

static RzList /*<RzBinReloc *>*/ *relocs(RzBinFile *bf) {
	return rz_bin_le_get_relocs(bf->o->bin_obj);
}

static RzBinInfo *info(RzBinFile *bf) {
	RzBinInfo *info = RZ_NEW0(RzBinInfo);
	if (info) {
		rz_bin_le_obj_t *bin = bf->o->bin_obj;
		LE_image_header *h = bin->header;
		info->bits = 32;
		info->type = strdup(bin->type);
		info->cpu = strdup(bin->cpu);
		info->os = strdup(bin->os);
		info->arch = strdup(bin->arch);
		info->file = strdup(bin->filename ? bin->filename : "");
		info->big_endian = h->worder;
		info->has_va = true;
		info->baddr = 0;
	}
	return info;
}

static RzList /*<RzBinString *>*/ *strings(RzBinFile *bf) {
	return rz_bin_file_strings(bf, 0, false);
}

RzBinPlugin rz_bin_plugin_le = {
	.name = "le",
	.desc = "LE/LX format plugin",
	.author = "GustavoLCR",
	.license = "LGPL3",
	.check_buffer = &check_buffer,
	.load_buffer = &load_buffer,
	.destroy = &destroy,
	.info = &info,
	.header = &header,
	.maps = &rz_bin_maps_of_file_sections,
	.sections = &sections,
	.entries = &entries,
	.symbols = &symbols,
	.imports = &imports,
	.strings = &strings,
	.libs = &libs,
	.relocs = &relocs,
	.minstrlen = 4
	// .regstate = &regstate
};

#ifndef RZ_PLUGIN_INCORE
RZ_API RzLibStruct rizin_plugin = {
	.type = RZ_LIB_TYPE_BIN,
	.data = &rz_bin_plugin_le,
	.version = RZ_VERSION
};
#endif
