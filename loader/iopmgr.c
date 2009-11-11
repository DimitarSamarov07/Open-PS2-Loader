/*
  Copyright 2009, Ifcaro & jimmikaelkael
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
  
  Some parts of the code are taken from HD Project by Polo
*/

#include "loader.h"
#include "iopmgr.h"

extern void *imgdrv_irx;
extern int size_imgdrv_irx;

extern void *usbd_irx;
extern int size_usbd_irx;

extern void *usbhdfsd_irx;
extern int size_usbhdfsd_irx;

extern void *cdvdman_irx;
extern int size_cdvdman_irx;

extern void *eesync_irx;
extern int size_eesync_irx;

extern void *isofs_irx;
extern int size_isofs_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *ps2ip_irx;
extern int size_ps2ip_irx;

extern void *ps2smap_irx;
extern int size_ps2smap_irx;

extern void *netlog_irx;
extern int size_netlog_irx;

extern void *smbman_irx;
extern int size_smbman_irx;

extern void *dummy_irx;
extern int size_dummy_irx;

/*----------------------------------------------------------------------------------------*/
void list_modules(void)
{
	int c = 0;
	smod_mod_info_t info;
	u8 name[60];

	if (!smod_get_next_mod(0, &info))
		return;
	scr_printf("\n\t List of modules currently loaded in the system:\n\n");
	do {
		smem_read(info.name, name, sizeof name);
		if (!(c & 1)) scr_printf(" ");
		scr_printf("   \t%-21.21s  %2d  %3x%c", name, info.id, info.version, (++c & 1) ? ' ' : '\n');
	} while (smod_get_next_mod(&info, &info) != 0);
}

/*----------------------------------------------------------------------------------------*/
/* Reset IOP processor. This fonction hook reset iop if an update sequence is requested.  */
/*----------------------------------------------------------------------------------------*/
int New_Reset_Iop(const char *arg, int flag){
	void   *rom_iop;
	int     i, j, r, fd=0;
	ioprp_t ioprp_img;
	char    ioprp_path[0x50];
	char    tmp[0x50];	
	int eeloadcnf_reset = 0;		
	
	GS_BGCOLOUR = 0xFF00FF;
	
	iop_reboot_count++;
	
	SifInitRpc(0);
	
	if (iop_reboot_count > 2) { 
		// above 2nd IOP reset, we can't be sure we'll be able to read IOPRP without 
		// Resetting IOP (game IOPRP is loaded at 2nd reset), so...
		// Reseting IOP.
		SifExitRpc();
		
		while (!Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0)) {;}
		while (!Sync_Iop()){;}
	
		fioExit();
		SifExitIopHeap();
		SifInitRpc(0);
		SifInitIopHeap();
		LoadFileInit();
		Sbv_Patch();

		GS_BGCOLOUR = 0x00A5FF;
			
		if (GameMode == USB_MODE) {
			LoadIRXfromKernel(usbd_irx, size_usbd_irx, 0, NULL);
			LoadIRXfromKernel(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL);
			delay(3);
		}
		else if (GameMode == ETH_MODE) {
			LoadIRXfromKernel(ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
			LoadIRXfromKernel(ps2ip_irx, size_ps2ip_irx, 0, NULL);
			LoadIRXfromKernel(ps2smap_irx, size_ps2smap_irx, g_ipconfig_len, g_ipconfig);
			//LoadIRXfromKernel(netlog_irx, size_netlog_irx, 0, NULL);
			LoadIRXfromKernel(smbman_irx, size_smbman_irx, 0, NULL);
		}
		LoadIRXfromKernel(isofs_irx, size_isofs_irx, 0, NULL);
	}

	// check for reboot with IOPRP from cdrom
	char *ioprp_pattern = "rom0:UDNL cdrom";
	for (i=0; i<0x50; i++) {
		for (j=0; j<15; j++) {
			if (arg[i+j] != ioprp_pattern[j])
				break;
		}
		if (j == 15) {
			// get IOPRP img file path
			strcpy(ioprp_path, &arg[i+10]);
			break;
		}
	}

	// check for reboot with IOPRP from rom	
	char *eeloadcnf_pattern = "rom0:UDNL rom";
	for (i=0; i<0x50; i++) {
		for (j=0; j<13; j++) {
			if (arg[i+j] != eeloadcnf_pattern[j])
				break;
		}
		if (j == 13) {
			strcpy(ioprp_path, &arg[i+10]);
			eeloadcnf_reset = 1;
			break;
		}
	}	

	if (iop_reboot_count > 2) {
		// we have loaded isofs, but not cdvdman replacement so...
		// replacing cdrom in elf path by iso
		if (strstr(ioprp_path, "cdrom")) {
			strcpy(tmp, "iso");
			strcat(tmp, &ioprp_path[5]);
			strcpy(ioprp_path, tmp);
		}
	}

	fioInit();		
	fd = open(ioprp_path, O_RDONLY);
	if (fd < 0){
		GS_BGCOLOUR = 0x000080;
		while (1){;}
	}

	ioprp_img.size_in = lseek(fd, 0, SEEK_END);
	if (ioprp_img.size_in % 0x40)
		ioprp_img.size_in = (ioprp_img.size_in & 0xffffffc0) + 0x40;
	lseek(fd, 0, SEEK_SET);

	ioprp_img.data_in  = (void *)g_buf;	
	if (eeloadcnf_reset)
		ioprp_img.data_out = (void *)(g_buf+102400);
	else {	
		ioprp_img.data_out = ioprp_img.data_in;
		ioprp_img.size_out = ioprp_img.size_in;
	}

	read(fd, ioprp_img.data_in , ioprp_img.size_in);
	
	close(fd);
	fioExit();
			
	if (eeloadcnf_reset) {
		r = Patch_EELOADCNF_Img(&ioprp_img);
		if (r == 0){
			GS_BGCOLOUR = 0x00FF00;
			while (1){;}
		}
	}
	else { 
		Patch_Mod(&ioprp_img, "CDVDMAN", cdvdman_irx, size_cdvdman_irx);
		Patch_Mod(&ioprp_img, "CDVDFSV", dummy_irx, size_dummy_irx);
		Patch_Mod(&ioprp_img, "EESYNC", eesync_irx, size_eesync_irx);
	}
	
	SifExitRpc();
	SifExitIopHeap();
	LoadFileExit();

	// Reseting IOP.
	while (!Reset_Iop("rom0:UDNL rom0:EELOADCNF", 0)) {;}
	while (!Sync_Iop()){;}

	SifInitRpc(0);
	SifInitIopHeap();
	LoadFileInit();
	Sbv_Patch();
	
	SifInitIopHeap();
	LoadFileExit();
	Sbv_Patch();	
	
	rom_iop = SifAllocIopHeap(ioprp_img.size_out);
	
	if (rom_iop){
		CopyToIop(ioprp_img.data_out, ioprp_img.size_out, rom_iop);
		
		// get back imgdrv from kernel ram		
		DIntr();
		ee_kmode_enter();
		memcpy(g_buf, imgdrv_irx, size_imgdrv_irx);
		ee_kmode_exit();
		EIntr();

		*(u32*)(&g_buf[0x180]) = (u32)rom_iop;
		*(u32*)(&g_buf[0x184]) = ioprp_img.size_out;
		FlushCache(0);

		LoadMemModule(g_buf, size_imgdrv_irx, 0, NULL);
		LoadModuleAsync("rom0:UDNL", 5, "img0:");

		SifExitRpc();
		LoadFileExit();

		DIntr();
		ee_kmode_enter();
		Old_SifSetReg(SIF_REG_SMFLAG, 0x10000);
		Old_SifSetReg(SIF_REG_SMFLAG, 0x20000);
		Old_SifSetReg(0x80000002, 0);
		Old_SifSetReg(0x80000000, 0);
		ee_kmode_exit();
		EIntr();
	}else{
		GS_BGCOLOUR = 0xFF0000;
		while (1){;}
	}

	while (!Sync_Iop()) {;}

	SifExitIopHeap();
	SifInitRpc(0);
	SifInitIopHeap();
	LoadFileInit();
	Sbv_Patch();
	
	GS_BGCOLOUR = 0x00FFFF;
	
	if (GameMode == USB_MODE) {
		LoadIRXfromKernel(usbd_irx, size_usbd_irx, 0, NULL);
		LoadIRXfromKernel(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL);
		delay(3);
	}
	else if (GameMode == ETH_MODE) {
		LoadIRXfromKernel(ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
		LoadIRXfromKernel(ps2ip_irx, size_ps2ip_irx, 0, NULL);
		LoadIRXfromKernel(ps2smap_irx, size_ps2smap_irx, g_ipconfig_len, g_ipconfig);
		//LoadIRXfromKernel(netlog_irx, size_netlog_irx, 0, NULL);
		LoadIRXfromKernel(smbman_irx, size_smbman_irx, 0, NULL);
	}	
	
	LoadIRXfromKernel(isofs_irx, size_isofs_irx, 0, NULL);
	FlushCache(0);	
	
	SifExitRpc();
	SifExitIopHeap();
	LoadFileExit();

	// we have 4 SifSetReg calls to skip in ELF's SifResetIop, not when we use it ourselves
	if (set_reg_disabled)
		set_reg_hook = 4;

 return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Reset IOP processor. This fonction replace SifIopReset from Ps2Sdk                     */
/*----------------------------------------------------------------------------------------*/
int Reset_Iop(const char *arg, int flag){
	SifCmdResetData reset_pkt;
	struct t_SifDmaTransfer dmat;

	SifExitRpc();

	SifStopDma();

	memset(&reset_pkt, 0, sizeof(SifCmdResetData));

	reset_pkt.chdr.psize = sizeof(SifCmdResetData);
	reset_pkt.chdr.fcode = 0x80000003;

	reset_pkt.flag = flag;

	if (arg != NULL){
		strncpy(reset_pkt.arg, arg, 0x50-1);
		reset_pkt.arg[0x50-1] = '\0';
		reset_pkt.size = strlen(reset_pkt.arg) + 1;
	}

	dmat.src  = &reset_pkt;
	dmat.dest = (void *)SifGetReg(0x80000000); // SIF_REG_SUBADDR
	dmat.size = sizeof reset_pkt;
	dmat.attr = 0x40 | SIF_DMA_INT_O;
	SifWriteBackDCache(&reset_pkt, sizeof reset_pkt);

	DIntr();
	ee_kmode_enter();
	if (!Old_SifSetDma(&dmat, 1)){
		ee_kmode_exit();
		EIntr();
		return 0;
	}

	Old_SifSetReg(SIF_REG_SMFLAG, 0x10000);
	Old_SifSetReg(SIF_REG_SMFLAG, 0x20000);
	Old_SifSetReg(0x80000002, 0);
	Old_SifSetReg(0x80000000, 0);
	ee_kmode_exit();
	EIntr();

	return 1;
}

/*----------------------------------------------------------------------------------------*/
/* Synchronize IOP processor. This fonction replace SifIopReset from Ps2Sdk                     */
/*----------------------------------------------------------------------------------------*/
int Sync_Iop(void){
	if (SifGetReg(SIF_REG_SMFLAG) & 0x40000){
		return 1;
	}
	return 0;
}