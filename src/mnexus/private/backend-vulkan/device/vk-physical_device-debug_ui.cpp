// TU header --------------------------------------------
#include "backend-vulkan/device/vk-physical_device-debug_ui.h"

#if MNEXUS_HAVE_DEAR_IMGUI

// external headers -------------------------------------
#include "imgui.h"

namespace mnexus_backend::vulkan::debug_ui {

namespace {

char const* H265LevelIdcToCStr(StdVideoH265LevelIdc level) {
  switch (level) {
  case STD_VIDEO_H265_LEVEL_IDC_1_0: return "1.0";
  case STD_VIDEO_H265_LEVEL_IDC_2_0: return "2.0";
  case STD_VIDEO_H265_LEVEL_IDC_2_1: return "2.1";
  case STD_VIDEO_H265_LEVEL_IDC_3_0: return "3.0";
  case STD_VIDEO_H265_LEVEL_IDC_3_1: return "3.1";
  case STD_VIDEO_H265_LEVEL_IDC_4_0: return "4.0";
  case STD_VIDEO_H265_LEVEL_IDC_4_1: return "4.1";
  case STD_VIDEO_H265_LEVEL_IDC_5_0: return "5.0";
  case STD_VIDEO_H265_LEVEL_IDC_5_1: return "5.1";
  case STD_VIDEO_H265_LEVEL_IDC_5_2: return "5.2";
  case STD_VIDEO_H265_LEVEL_IDC_6_0: return "6.0";
  case STD_VIDEO_H265_LEVEL_IDC_6_1: return "6.1";
  case STD_VIDEO_H265_LEVEL_IDC_6_2: return "6.2";
  default: return "?";
  }
}

void ShowVkExtent2DBullet(char const* label, VkExtent2D const& extent) {
  ImGui::BulletText("%s: %u x %u", label, extent.width, extent.height);
}

void ShowVkVideoCapabilitiesKHRUi(VkVideoCapabilitiesKHR const& caps) {
  ImGui::BulletText("flags: %s", string_VkVideoCapabilityFlagsKHR(caps.flags).c_str());
  ImGui::BulletText("minBitstreamBufferOffsetAlignment: %llu",
                    static_cast<unsigned long long>(caps.minBitstreamBufferOffsetAlignment));
  ImGui::BulletText("minBitstreamBufferSizeAlignment: %llu",
                    static_cast<unsigned long long>(caps.minBitstreamBufferSizeAlignment));
  ShowVkExtent2DBullet("pictureAccessGranularity", caps.pictureAccessGranularity);
  ShowVkExtent2DBullet("minCodedExtent", caps.minCodedExtent);
  ShowVkExtent2DBullet("maxCodedExtent", caps.maxCodedExtent);
  ImGui::BulletText("maxDpbSlots: %u", caps.maxDpbSlots);
  ImGui::BulletText("maxActiveReferencePictures: %u", caps.maxActiveReferencePictures);
  ImGui::BulletText("stdHeaderVersion: %s (spec %u)",
                    caps.stdHeaderVersion.extensionName,
                    caps.stdHeaderVersion.specVersion);
}

void ShowVkVideoFormatPropertiesKHRUi(VkVideoFormatPropertiesKHR const& fp) {
  ImGui::BulletText("format: %s", string_VkFormat(fp.format));
  ImGui::BulletText("componentMapping: r=%s g=%s b=%s a=%s",
                    string_VkComponentSwizzle(fp.componentMapping.r),
                    string_VkComponentSwizzle(fp.componentMapping.g),
                    string_VkComponentSwizzle(fp.componentMapping.b),
                    string_VkComponentSwizzle(fp.componentMapping.a));
  ImGui::BulletText("imageType: %s", string_VkImageType(fp.imageType));
  ImGui::BulletText("imageTiling: %s", string_VkImageTiling(fp.imageTiling));
  ImGui::BulletText("imageCreateFlags: %s", string_VkImageCreateFlags(fp.imageCreateFlags).c_str());
  ImGui::BulletText("imageUsageFlags: %s", string_VkImageUsageFlags(fp.imageUsageFlags).c_str());
}

} // namespace

void ShowVideoDecodeH265PropertiesUi(VideoDecodeH265Properties const& props) {
  if (ImGui::TreeNode("VkVideoCapabilitiesKHR")) {
    ShowVkVideoCapabilitiesKHRUi(props.coding_capabilities);
    ImGui::TreePop();
  }

  ImGui::BulletText("decode_flags: %s",
                    string_VkVideoDecodeCapabilityFlagsKHR(props.decode_flags).c_str());
  ImGui::BulletText("max_level_idc: %s", H265LevelIdcToCStr(props.max_level_idc));

  if (ImGui::TreeNode("VkVideoFormatPropertiesKHR (first)")) {
    ShowVkVideoFormatPropertiesKHRUi(props.format_properties);
    ImGui::TreePop();
  }
}

void ShowVideoDecodeH265CapabilitiesUi(VideoDecodeH265Capabilities const& caps) {
  auto show = [](char const* label, std::optional<VideoDecodeH265Properties> const& props) {
    if (props.has_value()) {
      if (ImGui::TreeNode(label)) {
        ShowVideoDecodeH265PropertiesUi(*props);
        ImGui::TreePop();
      }
    } else {
      ImGui::Text("%s:", label);
      ImGui::SameLine();
      ImGui::TextDisabled("not supported");
    }
  };

  show("Main (8-bit)", caps.main);
  show("Main 10 (8-bit)", caps.main10_8bit);
  show("Main 10 (10-bit)", caps.main10_10bit);
}

void ShowVideoCodingCapabilitiesUi(VideoCodingCapabilities const& caps) {
  if (ImGui::TreeNode("Decode H.265")) {
    ShowVideoDecodeH265CapabilitiesUi(caps.decode_h265);
    ImGui::TreePop();
  }
}

} // namespace mnexus_backend::vulkan::debug_ui

#endif // MNEXUS_HAVE_DEAR_IMGUI
