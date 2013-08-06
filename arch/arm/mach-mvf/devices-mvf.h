/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <mach/mvf.h>
#include <mach/devices-common.h>

extern const struct imx_imx_uart_1irq_data mvf_imx_uart_data[] __initconst;
#define mvf_add_imx_uart(id, pdata)	\
	imx_add_imx_uart_1irq(&mvf_imx_uart_data[id], pdata)

extern const struct imx_snvs_rtc_data mvf_snvs_rtc_data __initconst;
#define mvf_add_snvs_rtc()	\
	imx_add_snvs_rtc(&mvf_snvs_rtc_data)

extern const struct imx_fec_data mvf_fec_data[] __initconst;
#define mvf_add_fec(id, pdata)	\
	imx_add_fec(id, &mvf_fec_data[id], pdata)

extern const struct imx_sdhci_esdhc_imx_data
mvf_sdhci_esdhc_imx_data[] __initconst;
#define mvf_add_sdhci_esdhc_imx(id, pdata)	\
	imx_add_sdhci_esdhc_imx(&mvf_sdhci_esdhc_imx_data[id], pdata)

extern const struct imx_spi_imx_data mvf_dspi_data[] __initconst;
#define mvf_add_dspi(id, pdata)	\
	mvf_add_spi_mvf(&mvf_dspi_data[id], pdata)

extern const struct imx_spi_imx_data mvf_qspi_data[] __initconst;
#define mvf_add_qspi(id, pdata)	\
	mvf_add_spi_mvf(&mvf_qspi_data[id], pdata)

extern const struct imx_imx_i2c_data mvf_i2c_data[] __initconst;
#define mvf_add_imx_i2c(id, pdata)	\
	imx_add_imx_i2c(&mvf_i2c_data[id], pdata)

extern const struct imx_mxc_nand_data mvf_nand_data __initconst;
#define mvf_add_nand(pdata)	\
	imx_add_mxc_nand(&mvf_nand_data, pdata)

#if (defined CONFIG_MACH_PCM052 || defined CONFIG_MACH_PCL052)
extern const struct imx_fsl_usb2_udc_data mvf_fsl_usb2_udc_data[] __initconst;
#define mvf_add_fsl_usb2_udc(id, pdata)	\
	imx_add_fsl_usb2_udc(&mvf_fsl_usb2_udc_data[id], pdata)

extern const struct imx_mxc_ehci_data mvf_mxc_ehci_otg_data[] __initconst;
#define mvf_add_fsl_ehci_otg(id, pdata)	\
	imx_add_fsl_ehci(&mvf_mxc_ehci_otg_data[id], pdata)
#else
extern const struct imx_fsl_usb2_udc_data mvf_fsl_usb2_udc_data __initconst;
#define mvf_add_fsl_usb2_udc(pdata)	\
	imx_add_fsl_usb2_udc(&mvf_fsl_usb2_udc_data, pdata)

extern const struct imx_mxc_ehci_data mvf_mxc_ehci_otg_data __initconst;
#define mvf_add_fsl_ehci_otg(pdata)	\
	imx_add_fsl_ehci(&mvf_mxc_ehci_otg_data, pdata)
#endif

extern const struct imx_mxc_ehci_data mvf_mxc_ehci_hs_data[] __initconst;
#if (defined CONFIG_MACH_PCM052 || defined CONFIG_MACH_PCL052)
#define mvf_add_fsl_ehci_hs(id, pdata) \
	imx_add_fsl_ehci(&mvf_mxc_ehci_hs_data[id], pdata)
#else
#define mvf_add_fsl_ehci_hs(id, pdata)	\
	imx_add_fsl_ehci(&mvf_mxc_ehci_hs_data[id - 1], pdata)
#endif

extern const struct imx_fsl_usb2_otg_data mvf_fsl_usb2_otg_data __initconst;
#define mvf_add_fsl_usb2_otg(pdata)	\
	imx_add_fsl_usb2_otg(&mvf_fsl_usb2_otg_data, pdata)

extern
const struct imx_fsl_usb2_wakeup_data mvf_fsl_otg_wakeup_data[] __initconst;
#if (defined CONFIG_MACH_PCM052 || defined CONFIG_MACH_PCL052)
#define mvf_add_fsl_usb2_ehci_otg_wakeup(id, pdata)	\
	imx_add_fsl_usb2_wakeup(&mvf_fsl_otg_wakeup_data[id], pdata)
#define mvf_add_fsl_usb2_udc_wakeup(id, pdata) \
	imx_add_fsl_usb2_wakeup(&mvf_fsl_otg_wakeup_data[id], pdata)
#else
#define mvf_add_fsl_usb2_ehci_otg_wakeup(pdata)	\
	imx_add_fsl_usb2_wakeup(&mvf_fsl_otg_wakeup_data[1], pdata)
#define mvf_add_fsl_usb2_udc_wakeup(pdata) \
	imx_add_fsl_usb2_wakeup(&mvf_fsl_otg_wakeup_data[0], pdata)
#endif

extern
const struct imx_fsl_usb2_wakeup_data mvf_fsl_hs_wakeup_data[] __initconst;
#if (defined CONFIG_MACH_PCM052 || defined CONFIG_MACH_PCL052)
#define mvf_add_fsl_usb2_hs_wakeup(id, pdata)  \
	imx_add_fsl_usb2_wakeup(&mvf_fsl_hs_wakeup_data[id], pdata)
#else
#define mvf_add_fsl_usb2_hs_wakeup(id, pdata)	\
	imx_add_fsl_usb2_wakeup(&mvf_fsl_hs_wakeup_data[id - 1], pdata)
#endif

extern const struct imx_imx_esai_data mvf_imx_esai_data[] __initconst;
#define mvf_add_imx_esai(id, pdata) \
	imx_add_imx_esai(&mvf_imx_esai_data[id], pdata)

extern const struct imx_viv_gpu_data mvf_gpu_data __initconst;

extern const struct imx_ahci_data mvf_ahci_data __initconst;
#define mvf_add_ahci(id, pdata)   \
	imx_add_ahci(&mvf_ahci_data, pdata)

extern const struct imx_imx_ssi_data mvf_imx_ssi_data[] __initconst;
#define mvf_add_imx_ssi(id, pdata)            \
	imx_add_imx_ssi(&mvf_imx_ssi_data[id], pdata)

extern const struct imx_ipuv3_data mvf_ipuv3_data[] __initconst;
#define mvf_add_ipuv3(id, pdata)  imx_add_ipuv3(id, &mvf_ipuv3_data[id], pdata)
#define mvf_add_ipuv3fb(id, pdata)  imx_add_ipuv3_fb(id, pdata)

#define mvf_add_lcdif(pdata)	\
	platform_device_register_resndata(NULL, "mxc_lcdif",\
			0, NULL, 0, pdata, sizeof(*pdata));

extern const struct imx_ldb_data mvf_ldb_data __initconst;
#define mvf_add_ldb(pdata) \
	imx_add_ldb(&mvf_ldb_data, pdata);

#define mvf_add_v4l2_output(id)	\
	platform_device_register_resndata(NULL, "mxc_v4l2_output",\
			id, NULL, 0, NULL, 0);

#define mvf_add_v4l2_capture(id)	\
	platform_device_register_resndata(NULL, "mxc_v4l2_capture",\
			id, NULL, 0, NULL, 0);

extern const struct imx_mxc_hdmi_data mvf_mxc_hdmi_data __initconst;
#define mvf_add_mxc_hdmi(pdata)	\
	imx_add_mxc_hdmi(&mvf_mxc_hdmi_data, pdata)

extern const struct imx_mxc_hdmi_core_data mvf_mxc_hdmi_core_data __initconst;
#define mvf_add_mxc_hdmi_core(pdata)	\
	imx_add_mxc_hdmi_core(&mvf_mxc_hdmi_core_data, pdata)

extern const struct imx_vpu_data mvf_vpu_data __initconst;
#define mvf_add_vpu() imx_add_vpu(&mvf_vpu_data)

extern const struct mvf_dcu_data mvfa5_dcu_data[] __initconst;
#define mvfa5_add_dcu(id, pdata) mvf_add_dcu(id, &mvfa5_dcu_data[id], pdata)

extern const struct mvf_sai_data mvfa5_sai_data[] __initconst;
#define mvfa5_add_sai(id, pdata)            \
	mvf_add_sai(id, &mvfa5_sai_data[id], pdata)

extern const struct imx_otp_data mvf_otp_data __initconst;
#define mvf_add_otp() \
	imx_add_otp(&mvf_otp_data)

extern const struct imx_viim_data mvf_viim_data __initconst;
#define mvf_add_viim() \
	imx_add_viim(&mvf_viim_data)

extern const struct imx_imx2_wdt_data mvf_imx2_wdt_data[] __initconst;
#define mvf_add_imx2_wdt(id, pdata)   \
	imx_add_imx2_wdt(&mvf_imx2_wdt_data[id])

extern const struct mvf_adc_data mvfa5_adc_data[] __initconst;
#define mvf_add_adc(id)	\
	mvf_add_adcdev(&mvfa5_adc_data[id])

extern const struct imx_imx2_wdt_data fsl_imx2_wdt_data[] __initconst;
#define mvf_add_wdt(id)   \
	imx_add_imx2_wdt(&fsl_imx2_wdt_data[id])

extern const struct imx_pm_imx_data mvf_pm_imx_data __initconst;
#define mvf_add_pm_imx(id, pdata)	\
	imx_add_pm_imx(&mvf_pm_imx_data, pdata)

extern const struct imx_imx_asrc_data mvf_imx_asrc_data[] __initconst;
#define mvf_add_asrc(pdata)	\
	imx_add_imx_asrc(mvf_imx_asrc_data, pdata)

extern const struct imx_dvfs_core_data mvf_dvfs_core_data __initconst;
#define mvf_add_dvfs_core(pdata)	\
	imx_add_dvfs_core(&mvf_dvfs_core_data, pdata)

extern const struct imx_viv_gpu_data mvf_gc2000_data __initconst;
extern const struct imx_viv_gpu_data mvf_gc320_data __initconst;
extern const struct imx_viv_gpu_data mvf_gc355_data __initconst;

extern const struct imx_mxc_pwm_data mvf_mxc_pwm_data[] __initconst;
#define mvf_add_mxc_pwm(id)	\
	imx_add_mxc_pwm(&mvf_mxc_pwm_data[id])

#define mvf_add_mxc_pwm_backlight(id, pdata)	   \
	platform_device_register_resndata(NULL, "pwm-backlight",\
			id, NULL, 0, pdata, sizeof(*pdata));

extern const struct imx_spdif_data mvf_imx_spdif_data __initconst;
#define mvf_add_spdif(pdata)	imx_add_spdif(&mvf_imx_spdif_data, pdata)

extern const struct imx_spdif_dai_data mvf_spdif_dai_data __initconst;
#define mvf_add_spdif_dai()	imx_add_spdif_dai(&mvf_spdif_dai_data)

#define mvf_add_spdif_audio_device(pdata)	imx_add_spdif_audio_device()

#define mvf_add_hdmi_soc() imx_add_hdmi_soc()
extern const struct imx_hdmi_soc_data mvf_imx_hdmi_soc_dai_data __initconst;
#define mvf_add_hdmi_soc_dai() \
	imx_add_hdmi_soc_dai(&mvf_imx_hdmi_soc_dai_data)

extern const struct imx_mipi_dsi_data mvf_mipi_dsi_data __initconst;
#define mvf_add_mipi_dsi(pdata)	\
	imx_add_mipi_dsi(&mvf_mipi_dsi_data, pdata)

extern const struct imx_flexcan_data mvf_flexcan_data[] __initconst;
#define mvf_add_flexcan(id, pdata)	\
	imx_add_flexcan(&mvf_flexcan_data[id], pdata)
#define mvf_add_flexcan0(pdata)	mvf_add_flexcan(0, pdata)
#define mvf_add_flexcan1(pdata)	mvf_add_flexcan(1, pdata)

extern const struct imx_mipi_csi2_data mvf_mipi_csi2_data __initconst;
#define mvf_add_mipi_csi2(pdata)	\
	imx_add_mipi_csi2(&mvf_mipi_csi2_data, pdata)

extern const struct imx_perfmon_data mvf_perfmon_data[] __initconst;
#define mvf_add_perfmon(id) \
	imx_add_perfmon(&mvf_perfmon_data[id])

extern const struct imx_mxc_mlb_data mvf_mxc_mlb150_data __initconst;
#define mvf_add_mlb150(pdata)        \
	imx_add_mlb(pdata)

extern const struct imx_pxp_data mvf_pxp_data __initconst;
#define mvf_add_imx_pxp()   \
	imx_add_imx_pxp(&mvf_pxp_data)

#define mvf_add_imx_pxp_client()   \
	imx_add_imx_pxp_client()

extern const struct imx_epdc_data mvf_epdc_data __initconst;
#define mvf_add_imx_epdc(pdata)	\
	imx_add_imx_epdc(&mvf_epdc_data, pdata)

#define mvf_add_pwm_leds(pdata) \
	imx_add_platform_device("leds_pwm", 0, NULL, 0, pdata, sizeof(*pdata))

extern const struct mvf_caam_data mvf_caam_data __initconst;
#define  mvf700_add_caam() \
	mvf_add_caam(&mvf_caam_data)
