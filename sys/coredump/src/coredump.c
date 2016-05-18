/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <hal/hal_bsp.h>
#include <hal/flash_map.h>
#include <bootutil/image.h>
#include <imgmgr/imgmgr.h>
#include <coredump/coredump.h>

static void
dump_core_tlv(const struct flash_area *fa, uint32_t *off,
  struct coredump_tlv *tlv, void *data)
{
    flash_area_write(fa, *off, tlv, sizeof(*tlv));
    *off += sizeof(*tlv);

    flash_area_write(fa, *off, data, tlv->ct_len);
    *off += tlv->ct_len;
}

extern char *__vector_tbl_reloc__, *__StackTop;

void
dump_core(void *regs, int regs_sz)
{
    struct coredump_header hdr;
    struct coredump_tlv tlv;
    const struct flash_area *fa;
    struct image_version ver;
    uint8_t hash[IMGMGR_HASH_LEN];
    uint32_t off;

    if (flash_area_open(FLASH_AREA_CORE, &fa)) {
        return;
    }

    if (flash_area_read(fa, 0, &hdr, sizeof(hdr))) {
        return;
    }
    if (hdr.ch_magic == COREDUMP_MAGIC) {
        /*
         * Don't override corefile.
         */
        return;
    }

    if (flash_area_erase(fa, 0, fa->fa_size)) {
        return;
    }

    /*
     * First put in data, followed by the header.
     */
    tlv.ct_type = COREDUMP_TLV_REGS;
    tlv._pad = 0;
    tlv.ct_len = regs_sz;
    tlv.ct_off = 0;

    off = sizeof(hdr);
    dump_core_tlv(fa, &off, &tlv, regs);

    if (imgr_read_info(bsp_imgr_current_slot(), &ver, hash) == 0) {
        tlv.ct_type = COREDUMP_TLV_IMAGE;
        tlv.ct_len = IMGMGR_HASH_LEN;

        dump_core_tlv(fa, &off, &tlv, hash);
    }

    tlv.ct_type = COREDUMP_TLV_MEM;
    tlv.ct_len = (uint32_t)&__StackTop - (uint32_t)&__vector_tbl_reloc__;
    tlv.ct_off = (uint32_t)&__vector_tbl_reloc__;
    dump_core_tlv(fa, &off, &tlv, &__vector_tbl_reloc__);

    hdr.ch_magic = COREDUMP_MAGIC;
    hdr.ch_size = off;

    flash_area_write(fa, 0, &hdr, sizeof(hdr));
}
