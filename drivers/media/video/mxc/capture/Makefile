ifeq ($(CONFIG_VIDEO_MXC_IPU_CAMERA),y)
	obj-$(CONFIG_VIDEO_MXC_CAMERA) += mxc_v4l2_capture.o
	obj-$(CONFIG_MXC_IPU_PRP_VF_ADC) += ipu_prp_vf_adc.o
 	obj-$(CONFIG_MXC_IPU_PRP_VF_SDC) += ipu_prp_vf_sdc.o ipu_prp_vf_sdc_bg.o
	obj-$(CONFIG_MXC_IPU_PRP_ENC) += ipu_prp_enc.o ipu_still.o
endif

mx27_capture-objs := mx27_prphw.o mx27_prpsw.o mx27_v4l2_capture.o
obj-$(CONFIG_VIDEO_MXC_EMMA_CAMERA) += mx27_csi.o mx27_capture.o 

mc521da_camera-objs := mc521da.o sensor_clock.o
obj-$(CONFIG_MXC_CAMERA_MC521DA) += mc521da_camera.o

mt9v111_camera-objs := mt9v111.o sensor_clock.o
obj-$(CONFIG_MXC_CAMERA_MICRON111) += mt9v111_camera.o 

ov2640_camera-objs := ov2640.o sensor_clock.o
obj-$(CONFIG_MXC_CAMERA_OV2640) += ov2640_camera.o
