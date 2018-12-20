package=gmp
$(package)_version=6.1.2
$(package)_download_path=https://gmplib.org/download/gmp
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=5275bb04f4863a13516b2f39392ac5e272f5e1bb8057b18aec1c9b79d73d8fb2

define $(package)_set_vars
$(package)_config_opts=--disable-shared
$(package)_config_opts_mingw32=--enable-mingw
$(package)_config_opts_linux=--with-pic
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

