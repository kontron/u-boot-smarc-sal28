/*
 * Copyright 2013 Kontron Europe GmbH
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <i2c.h>
#include "emb_eep.h"

#ifdef CONFIG_EMB_EEP_I2C_EEPROM

#ifdef CONFIG_KEX_EEP_BOOTCOUNTER
#define CONFIG_EMB_EEP_WRITE
#endif

int EMB_EEP_I2C_EEPROM_BUS_NUM_1;
int EMB_EEP_I2C_EEPROM_BUS_NUM_2;

static int emb_eep_init (emb_eep_info *vpdi);
static char *emb_eep_find_mac_in_dmi_164 (emb_eep_info *vpdi, int string_num);
static char *emb_eep_find_entry_in_dmi (int eeprom_num, int dmi_num, int entry_num);

static char vpd_header[0x10];
static char vpd_block[CONFIG_EMB_EEP_I2C_EEPROM_SIZE];
static emb_eep_info vpdinfo;

#ifndef CONFIG_NET_RANDOM_ETHADDR
/*
 * This function is used to increment default MAC address in case more than one default
 * address is needed and only if no random address is generated
 */
static void increment_macstring(char *macstr, int inc)
{
        int i, macint;
        unsigned char mac[6];

        string_to_enetaddr(macstr, mac);
        if (inc) {
                /* add with carry */
                for (i=5 ; i>=0 ; i--) {
                        macint = (int)mac[i] + inc;
                        mac[i] = macint & 0xff;
                        if (macint > 255)
                                inc = 1;
                        else
                                break;
                }
        }
        sprintf(macstr, "%pM", mac);
}
#endif

static int i2c_read_emb (emb_eep_info *vpdi, int offset, unsigned char *buffer, int len)
{
	dm_i2c_read(vpdi->i2c_dev,
	            vpdi->eeprom_offset + offset,
	            (unsigned char *) &buffer[0],
	            len);

	return 0;
}

#ifdef CONFIG_EMB_EEP_WRITE
static int i2c_write_emb (emb_eep_info *vpdi, int offset, unsigned char *buffer, int len)
{
	int ret = 0;
	do {
		len--;
		dm_i2c_write(vpdi->i2c_dev,
		             vpdi->eeprom_offset + offset + len,
		             buffer + len,
		             1);
		udelay(5000);
	} while (len > 0);

	return ret;
}
#endif

static int emb_eep_get_block_size (emb_eep_info *vpdi, int idx)
{
	int block_size;
	uchar buffer[2];

	i2c_read_emb (vpdi, idx+1, (unsigned char *) &buffer[0],2);

	block_size = buffer[0] * 256 + buffer[1];

	block_size *= 2; /* block size in 2 bytes words */

	if (idx + block_size <= vpdi->max_size)
		return block_size;
	else
		return 0;
}

static int emb_eep_find_block(emb_eep_info *vpdi, int idx, int block_id)
{
	int block_size;
	uchar buffer[2];
	int block_len;

	if (idx == 0) {

		/* Calculate offset to first block */
		idx = 2 * (*(vpdi->header + 4));

		memset(buffer, 0, sizeof(buffer));
		i2c_read_emb (vpdi, idx, (unsigned char *) &buffer[0], 1);

		if (buffer[0] == block_id) {
			block_len = emb_eep_get_block_size(vpdi, idx);
			i2c_read_emb (vpdi, idx, (unsigned char *) vpdi->block,
			              block_len);

			return idx;
		}
#if 0
		if (buffer[0] == BLOCK_ID_CRC)
			return -1;
#endif
	}

	do {
		block_size = emb_eep_get_block_size(vpdi, idx);

		if (block_size <= 0)
			return -1;

		idx += block_size;

		i2c_read_emb (vpdi, idx, (unsigned char *) &buffer[0], 1);

		if (buffer[0] == block_id)
			break;
#if 0
		if (buffer[0] == BLOCK_ID_CRC)
			return -1;
#endif
	} while (idx < vpdi->max_size);

	block_size = emb_eep_get_block_size(vpdi, idx);
	if ((idx + block_size) < vpdi->max_size) {
		i2c_read_emb (vpdi, idx, (unsigned char *) vpdi->block,
		              block_size);

		return idx;
	}
	else
		return -1;
}

char * emb_eep_find_mac_in_dmi (int eeprom_num, int eth_num)
{
	emb_eep_info *vpdi;

	vpdi = &vpdinfo;
	vpdi->eeprom_num = eeprom_num;
	vpdi->block = &vpd_block[0];
	vpdi->header = &vpd_header[0];

	if (emb_eep_init (vpdi) != 0) {
		return NULL;
	}

	return emb_eep_find_mac_in_dmi_164(vpdi, eth_num);
}

char * emb_eep_find_string_in_dmi (int eeprom_num, int dmi_num, int string_num)
{
	return emb_eep_find_entry_in_dmi(eeprom_num, dmi_num, string_num);
}

static char * emb_eep_find_entry_in_dmi (int eeprom_num, int dmi_num, int entry_num)
{
	emb_eep_info *vpdi;

	int idx = 0, num, tmp_num;
	char *ptr, * block_end;
	int offset_string_index, offset_strings, max_strings;
	int found = 0;

	vpdi = &vpdinfo;
	vpdi->eeprom_num = eeprom_num;
	vpdi->block = &vpd_block[0];
	vpdi->header = &vpd_header[0];

	if (emb_eep_init (vpdi) != 0) {
		return NULL;
	}

	/*
	 * offsets count from SMBIOS dynamic block ID byte (0xd0) address.
	 * as entry_num is starting with 1, actual offset of  1st string
	 * index calculates to (offset-1) in block.
	 */
	switch (dmi_num) {
		case 2:
			offset_string_index = 7-1;
			offset_strings = 18;
			max_strings = 5;
			break;
		case 160:
			offset_string_index = 12-1;
			offset_strings = 16;
			max_strings = 4;
			break;
		case 161:
			offset_string_index = 0;
			if (entry_num == 1)
				offset_strings = BLOCK_161_BOOTCOUNTER;
			if (entry_num == 2)
				offset_strings = BLOCK_161_RUNNINGTIME;
			max_strings = 2;
			break;
			
		default:
			return NULL;
	}

	if ((entry_num < 1) || (entry_num > max_strings))
		return NULL;

	/*
	 * find SMBIOS Block with type dmi_num
	 */
	do {
		idx = emb_eep_find_block (vpdi, idx, BLOCK_ID_SMBIOS);
		debug("SMBIOS block found with index %d\n", idx);

		if (idx < 0)
			return NULL;

		if ((*(vpdi->block + 3)) == dmi_num) {
			found = 1;
			break;
		}

	} while (idx < vpdi->max_size);

	if (found == 0)
		return NULL;
	/*
	 * find string index of entry_num
	 */

	if (dmi_num != 161) {
		num = *(vpdi->block + offset_string_index + entry_num);
		if ((num == 0) | (num > max_strings))
			return NULL;
	} else {
		num = 1;
	}

	/*
	 * get offset to string
	 */
	ptr = vpdi->block + offset_strings;
	tmp_num = 1;
	while (tmp_num < num) {
		ptr += strlen (ptr) + 1;
		tmp_num ++;
	}

	/*
	 * check for plausibility
	 */
	block_end = vpdi->block + emb_eep_get_block_size(vpdi, idx);

	if (ptr < block_end)
		return ptr;
	else
		return NULL;

	return NULL;
}

#ifdef CONFIG_EMB_EEP_WRITE
/*
 * Use ofs parameter to write dedicated elements in a block
 */
static int emb_eep_set_get_dmi_block(int eeprom_num, int dmi_num, char *dmi_block, int ofs, int sz, int rw_flag)
{
	int idx = 0;
	emb_eep_info *vpdi;

	vpdi = &vpdinfo;
	vpdi->eeprom_num = eeprom_num;
	vpdi->block = vpd_block;
	vpdi->header = vpd_header;

	if (emb_eep_init(vpdi) != 0) {
		return -1;
	}

	/*
	 * find SMBIOS Block with type dmi_num
	 */
	do {
		idx = emb_eep_find_block (vpdi, idx, BLOCK_ID_SMBIOS);
		debug("SMBIOS block found with index %d\n", idx);

		if (idx < 0)
			return -1;

		if ((*(vpdi->block + 3)) == dmi_num) {
			if (rw_flag) {
				return (i2c_write_emb (vpdi, idx + ofs,
				            (unsigned char *)(dmi_block + ofs),
				            sz));
			}
			else {
				memcpy(dmi_block, vpdi->block, sz);
				return idx;
			}
		}
	} while (idx < vpdi->max_size);

	return -1;
}
#endif

static char *emb_eep_find_mac_in_dmi_164 (emb_eep_info *vpdi, int eth_num)
{
	int idx = 0, tmp_num, num;
	char *ptr, * block_end;
	int numOfMacs;
	/* find string num */
	if (eth_num < 1)
		return NULL;

	/*
	 * find SMBIOS Block with type 164
	 */
	do {
		idx = emb_eep_find_block (vpdi, idx, BLOCK_ID_SMBIOS);

		if (idx < 0)
			return NULL;

		else if ((*(vpdi->block + 3)) == 164)
			break;

	} while (idx < vpdi->max_size);

	numOfMacs = *(vpdi->block + 12);
	if (eth_num > numOfMacs)
		return NULL;

	/*
	 * get offset to string
	 */
	num = *(vpdi->block + 12 + eth_num);

	ptr = vpdi->block + 12 + eth_num + numOfMacs;
	tmp_num = 1;
	while (tmp_num < num) {
		ptr += strlen (ptr) + 1;
		tmp_num ++;
	}

	/*
	 * check for plausibility
	 */
	block_end = vpdi->block + emb_eep_get_block_size(vpdi, idx);

	if (ptr < block_end)
		return ptr;
	else
		return NULL;

	return ptr;
}


static void emb_eep_default_ethaddr (void)
{
#ifndef CONFIG_NET_RANDOM_ETHADDR
	char *e_ethaddr;

	e_ethaddr = env_get("ethaddr");
	if (e_ethaddr == NULL) {
		printf ("WARNING: ethaddr not found in environment, using default value\n");
		env_set  ("ethaddr", D_ETHADDR);
	}
#endif
}

static int emb_eep_check_header (emb_eep_info *vpdi)
{
	if (*(vpdi->header + 1) != '3'){
		printf ("emb_eep_check_header: 0x%x instead of 3\n",
		        *(vpdi->header + 1));
		return 0;
	}
	if (*(vpdi->header + 2) != 'P'){
		printf ("emb_eep_check_header: 0x%x instead of P\n",
		        *(vpdi->header + 2));
		return 0;
	}
#if 0	
	if (*(vpdi->header + 3) != 0x10)
		return 0;
#endif

	return 1;
}

static int emb_eep_init (emb_eep_info *vpdi)
{

	debug ("Initialize vpd_init, vpdi = 0x%p\n", (void *)vpdi);

	switch (vpdi->eeprom_num) {
#ifdef CONFIG_EMB_EEP_I2C_EEPROM_ADDR_1
	case 1:
		vpdi->eeprom_busnum = EMB_EEP_I2C_EEPROM_BUS_NUM_1;
		vpdi->eeprom_addr = CONFIG_EMB_EEP_I2C_EEPROM_ADDR_1;
		vpdi->eeprom_addrlen = CONFIG_EMB_EEP_I2C_EEPROM_ADDR_LEN_1;
		vpdi->eeprom_offset = CONFIG_EMB_EEP_I2C_EEPROM_OFFSET_1;
		break;
#endif
#ifdef CONFIG_EMB_EEP_I2C_EEPROM_ADDR_2
	case 2:
		vpdi->eeprom_busnum = EMB_EEP_I2C_EEPROM_BUS_NUM_2;
		vpdi->eeprom_addr = CONFIG_EMB_EEP_I2C_EEPROM_ADDR_2;
		vpdi->eeprom_addrlen = CONFIG_EMB_EEP_I2C_EEPROM_ADDR_LEN_2;
		vpdi->eeprom_offset = CONFIG_EMB_EEP_I2C_EEPROM_OFFSET_2;
		break;
#endif
	default:
		printf("Warning: EEPROM number %d not supported\n",
		       vpdi->eeprom_num);
		return -1;
	}

	/* read 6 bytes first */
	int ret;
	ret = i2c_get_chip_for_busnum(vpdi->eeprom_busnum,
	                              vpdi->eeprom_addr,
	                              vpdi->eeprom_addrlen,
	                              &(vpdi->i2c_dev));
	if (ret) {
		printf("EEPROM 0x%02x not found on bus %d\n",
		       vpdi->eeprom_addr, vpdi->eeprom_busnum);
		return -1;
	}

	i2c_read_emb(vpdi, 0, (unsigned char *)&vpdi->header[0], 6);

	if (!emb_eep_check_header(vpdi)) {
		printf ("WARNING: Embedded EEPROM header not valid, abort\n");
		return -1;
	}

	vpdi->max_size = 256 << (vpdi->header[5] & 0x7);
	if ( vpdi->max_size > CONFIG_EMB_EEP_I2C_EEPROM_SIZE) {
		printf ("Embedded EEPROM contents too large\n");
		return -1;
	}
		
	return 0;
}

/*
 * initialize environment from embedded EEPROM
 */
void emb_eep_init_r(int eeprom_num, int macs_expected)
{
	char *val;
	emb_eep_info *vpdi;
	int num;
	char ethname[32];
	char *e_ethaddr;
	char *v_ethaddr;
	uchar mac[6];

	vpdi = &vpdinfo;
	vpdi->block = &vpd_block[0];
	vpdi->header = &vpd_header[0];
	vpdi->eeprom_num = eeprom_num;

	if (emb_eep_init (vpdi) != 0) {
		emb_eep_default_ethaddr ();
		return;
	}

	printf("VPD:   Using device 0x%02x on I2C Bus %d\n",
	        vpdi->eeprom_addr, vpdi->eeprom_busnum);

	/*
	 * Import serial number to environment
	 * Block SMBIOS, type 2, string number 4
	 */
	val = emb_eep_find_string_in_dmi (eeprom_num, 2, 4);
	if (val != NULL)
		env_set("serial#", val);

	/*
	 * Import eth addresses to environment
	 */
	num = 0;
	while (true) {
		sprintf(ethname, num ? "eth%daddr" : "ethaddr", num);

		e_ethaddr = env_get(ethname);
		string_to_enetaddr(e_ethaddr, mac);
		if (!is_valid_ethaddr(mac))
			e_ethaddr = NULL;

		v_ethaddr = emb_eep_find_mac_in_dmi_164(vpdi, num+1);
		if (!v_ethaddr)
			/* no more MAC addresses in EEPROM, exit from loop */
			break;

		string_to_enetaddr(v_ethaddr, mac);
		if (!is_valid_ethaddr(mac)) {
			v_ethaddr = NULL;
		}

		/* must increment 'num' before loop can 'continue' */
		num++;
		/* use incremented D_ETHADDR if no random ethaddr available */
		if (!v_ethaddr && !e_ethaddr) {
#ifndef CONFIG_NET_RANDOM_ETHADDR
			char d_ethaddr[] = D_ETHADDR;

			increment_macstring(d_ethaddr, num-1);
			env_set(ethname, d_ethaddr);
			printf("       Using default %s\n", ethname);
			continue;
#else
			printf("*** Warning - %s not valid\n", ethname);
			continue;
#endif
		}

		if (e_ethaddr && (strcmp(v_ethaddr, e_ethaddr))) {
			printf("       Using ENV %s%s\n", ethname, v_ethaddr ?
			       " (overriding VPD)" : "");
			continue;
		}

		env_set(ethname, v_ethaddr);
	}

	if (macs_expected && (macs_expected != num)) {
		printf("*** Warning - Expected MAC address count (%d) differs "
		       "from VPD MAC count %d\n", macs_expected, num);
	}

	return;
}

#if defined(CONFIG_EMB_EEP_WRITE)
int emb_eep_update_bootcounter(int eeprom_num)
{
	int ret;
	uint64_t bc;
	emb_running_time_block_t emb_rt_block;

	ret = emb_eep_set_get_dmi_block(eeprom_num, 161,
	                                (char *)(&emb_rt_block),
					0,
	                                sizeof(emb_rt_block), 0);
	if (ret < 0) {
		printf("Could not read SMBIOS block 161\n");
		return ret;
	}

	memcpy(&bc, emb_rt_block.boot_counter, sizeof(uint64_t));
	bc = be64_to_cpu(bc);
	if ((bc & SIGN_64BIT) && (bc ^ -1ULL)) {
		printf("WARNING: Invalid boot counter\n");
		return 0;
	}

	bc = cpu_to_be64(bc + 1);
	memcpy(emb_rt_block.boot_counter, &bc, sizeof(uint64_t));

	/* write back only boot_counter bytes! */
	ret = emb_eep_set_get_dmi_block(eeprom_num, 161,
	                                (char *)(&emb_rt_block),
					BLOCK_161_BOOTCOUNTER,
	                                sizeof(uint64_t), 1);
	if (ret < 0) {
		printf("WARNING: Could not update boot counter\n");
		return ret;
	}

	return ret;
}
#endif

#endif /* CONFIG_EMB_EEP_I2C_EEPROM */
