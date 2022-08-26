#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>

#include "utils.h"
#include "media_stream.h"
#include "session_description.h"

struct SessionDescription {

  size_t size;

  int mdns_enabled:1;

  MediaDescription media_descriptions[MEDIA_DESCRIPTION_MAX_NUM];

  RtpMap rtp_map;

  int media_description_num;

  char content[SDP_MAX_SIZE];
};

static void session_description_parse_rtpmap(SessionDescription *sdp, const char *attribute_text) {

  //a=rtpmap:111 opus/48000/2
  //a=rtpmap:8 PCMA/8000
  //a=rtpmap:108 H264/90000
  int pt = 0;
  char codec[16];
  char *pt_start, *codec_start, *codec_end;

  pt_start = strstr(attribute_text, ":");
  codec_start = strstr(attribute_text, " ") + 1;
  codec_end = strstr(attribute_text, "/");

  if(!pt_start && !codec_start && !codec_end)
    return;

  pt = atoi(pt_start + 1);

  memset(codec, 0, sizeof(codec));
  strncpy(codec, codec_start, codec_end - codec_start);

  if(strcmp(codec, "H264") == 0) {
    // limit payload type larger than 96 for H264
    if(pt < 96)
      return;
    sdp->rtp_map.pt_h264 = pt;
  }
  else if(strcmp(codec, "PCMA") == 0) {
    sdp->rtp_map.pt_pcma = pt;
  }
  else if(strcmp(codec, "opus") == 0) {
    sdp->rtp_map.pt_opus = pt;
  }

}

SessionDescription* session_description_create(char *sdp_text) {

  char **splits;
  int i;

  SessionDescription *sdp = NULL;
  sdp = (SessionDescription*)calloc(1, sizeof(SessionDescription));
  if(!sdp || !sdp_text)
    return sdp;

  splits = g_strsplit(sdp_text, "\r\n", 512);
  for(i = 0; splits[i] != NULL; i++) {

    if(strstr(splits[i], "m=audio")) {

      LOG_DEBUG("Find audio media description (mid = %d)", sdp->media_description_num);
      sdp->media_descriptions[sdp->media_description_num++] = MEDIA_AUDIO;
    }
    else if(strstr(splits[i], "m=video")) {

      LOG_DEBUG("Find video media description (mid = %d)", sdp->media_description_num);
      sdp->media_descriptions[sdp->media_description_num++] = MEDIA_VIDEO;
    }
    else if(strstr(splits[i], "m=application")) {

      LOG_DEBUG("Find datachannel media description (mid = %d)", sdp->media_description_num);
      sdp->media_descriptions[sdp->media_description_num++] = MEDIA_DATACHANNEL;
    }

    if(strstr(splits[i], "rtpmap") != NULL) {
      session_description_parse_rtpmap(sdp, splits[i]);
    }

    if(strstr(splits[i], "candidate") != NULL && strstr(splits[i], "local") != NULL) {

      if(sdp->mdns_enabled) {
        char buf[256] = {0};
        if(session_description_update_mdns_of_candidate(splits[i], buf, sizeof(buf)) != -1) {
          session_description_append_newline(sdp, buf);
        }
      }
    }
    else {
      session_description_append_newline(sdp, splits[i]);
    }
  }

  return sdp;
}

void session_description_destroy(SessionDescription *sdp) {

  if(sdp) {
    free(sdp);
    sdp = NULL;
  }
}

void session_description_set_mdns_enabled(SessionDescription *sdp, int enabled) {

  sdp->mdns_enabled = enabled;
}

int session_description_update_mdns_of_candidate(char *candidate_src, char *candidate_dst, size_t size) {

  int token_of_mdns = 4;
  char hostname[128] = {0};
  char ipv4addr[64] = {0};
  char prefix[64] = {0};

  char *end;
  char *start = candidate_src;
  while(token_of_mdns > 0) {
    start = strstr(start, " ") + 1;
    token_of_mdns--;
  }
  end = strstr(start, " ");
  strncpy(hostname, start, end - start);
  if(utils_get_ipv4addr(hostname, ipv4addr, sizeof(ipv4addr)) < 0)
    return -1;
  strncpy(prefix, candidate_src, start - candidate_src);
  snprintf(candidate_dst, size, "%s%s%s", prefix, ipv4addr, end);
  return 0;
}

int session_description_append_newline(SessionDescription *sdp, const char *format, ...) {

  va_list argptr;
  char attribute[256] = {0};

  if(strstr(format, "=") == NULL)
    return -1;

  va_start(argptr, format);
  vsnprintf(attribute, sizeof(attribute), format, argptr);
  va_end(argptr);

  strcat(sdp->content, attribute);
  strcat(sdp->content, "\r\n");

  return 0;
}

int session_description_append(SessionDescription *sdp, const char *format, ...) {

  va_list argptr;
  char attribute[128] = {0};

  if(strstr(format, "=") == NULL)
    return -1;

  va_start(argptr, format);
  vsnprintf(attribute, sizeof(attribute), format, argptr);
  va_end(argptr);

  strcat(sdp->content, attribute);
  strcat(sdp->content, "\\r\\n");

  return 0;
}

char* session_description_get_content(SessionDescription *sdp) {

  return sdp->content; 
}


void session_description_add_codec(SessionDescription *sdp, MediaCodec codec,
 TransceiverDirection direction, const char *ufrag, const char *password, const char *fingerprint, int mid) {
#if 0
  switch(codec) {
    case CODEC_H264:
      session_description_append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 96 102");
      session_description_append(sdp, "a=rtcp-fb:102 nack");
      session_description_append(sdp, "a=rtcp-fb:102 nack pli");
      session_description_append(sdp, "a=fmtp:96 profile-level-id=42e01f;level-asymmetry-allowed=1");
      session_description_append(sdp, "a=fmtp:102 profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1");
      session_description_append(sdp, "a=fmtp:102 x-google-max-bitrate=6000;x-google-min-bitrate=2000;x-google-start-bitrate=4000");
      session_description_append(sdp, "a=rtpmap:96 H264/90000");
      session_description_append(sdp, "a=rtpmap:102 H264/90000");
      session_description_append(sdp, "a=ssrc:1 cname:pear");
      break;
    case CODEC_OPUS:
      session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 111");
      session_description_append(sdp, "a=rtcp-fb:111 nack");
      session_description_append(sdp, "a=rtpmap:111 opus/48000/2");
      session_description_append(sdp, "a=ssrc:2 cname:pear");
      break;
    case CODEC_PCMA:
      session_description_append(sdp, "m=audio 9 UDP/TLS/RTP/SAVP 8");
      session_description_append(sdp, "a=rtpmap:8 PCMA/8000");
      session_description_append(sdp, "a=ssrc:2 cname:pear");
      break;
    default:
      return;
  }
  session_description_append(sdp, "c=IN IP4 0.0.0.0");
  switch(direction) {
    case SENDRECV:
      session_description_append(sdp, "a=sendrecv");
      break;
    case RECVONLY:
      session_description_append(sdp, "a=recvonly");
      break;
    case SENDONLY:
      session_description_append(sdp, "a=sendonly");
      break;
    default:
      break;
  }
#endif
  session_description_append(sdp, "m=application 50712 UDP/DTLS/SCTP webrtc-datachannel");
  session_description_append(sdp, "a=mid:%d", mid);
  session_description_append(sdp, "a=sctp-port:5000");
  session_description_append(sdp, "a=max-message-size:262144");
//  session_description_append(sdp, "a=rtcp-mux");
  session_description_append(sdp, "a=ice-ufrag:%s", ufrag);
  session_description_append(sdp, "a=ice-pwd:%s", password);
  session_description_append(sdp, "a=ice-options:trickle");
  session_description_append(sdp, "a=fingerprint:sha-256 %s", fingerprint);
  session_description_append(sdp, "a=setup:passive");

}

uint32_t session_description_find_ssrc(const char *type, const char *sdp) {

  char *media_line = strstr(sdp, type);
  char *ssrc_pos = NULL;
  uint32_t ssrc = 0;

  if(media_line == NULL)
    return ssrc;
  ssrc_pos = strstr(media_line, "ssrc:");

  if(ssrc_pos == NULL)
    return ssrc;

  ssrc = strtoul(ssrc_pos + 5, NULL, 0);
  return ssrc;
}

MediaDescription* session_description_get_media_descriptions(SessionDescription *sdp, int *num) {

  *num = sdp->media_description_num;
  return sdp->media_descriptions;
}

RtpMap session_description_get_rtpmap(SessionDescription *sdp) {

  return sdp->rtp_map;
}