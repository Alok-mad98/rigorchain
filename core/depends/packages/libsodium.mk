package=libsodium
# Ed25519 signature verification is a consensus rule: keep this pin identical to
# tools/release/libsodium.lock. Tarball signed by the libsodium minisign key
# RWQf6LRCGA9i53mlYecO4IzT51TGPpvWucNSCh1CBM0QTaLn73Y7GFO3.
$(package)_version=1.0.22
$(package)_download_path=https://download.libsodium.org/libsodium/releases/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-static --disable-dependency-tracking --enable-option-checking
$(package)_config_opts_linux=--with-pic
$(package)_config_opts_freebsd=--with-pic
$(package)_config_opts_netbsd=--with-pic
$(package)_config_opts_openbsd=--with-pic
endef

define $(package)_preprocess_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub build-aux/
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -f lib/*.la
endef
