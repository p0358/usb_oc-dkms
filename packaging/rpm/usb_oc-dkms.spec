%if 0%{?fedora}
%global debug_package %{nil}
%endif
%global dkms_name usb_oc
%global version 1.0

Name:       %{dkms_name}-dkms
Version:    %{version}
Release:    1%{?dist}
Summary:    Kernel module for overclocking USB devices
Group:      System Environment/Kernel
License:    GPL-2.0-only
URL:        https://github.com/p0358/usb_oc-dkms
BuildArch:  noarch

# Source file:
Source0:    files.tar
#Source0:    %{URL}/archive/refs/tags/v%{version}.tar.gz

Requires:   dkms

%description
Kernel module for overclocking USB devices by overriding their bInterval on interrupt endpoint descriptors.

%prep
# List tar contents and unpack it
# (setup and other macros and their options are documented at https://rpm-packaging-guide.github.io/ btw)
tar tvf %{SOURCE0}
%setup -q -c
#% setup -q -c -n %{name}-%{version}

%build
# Empty; rpmlint recommends it is present anyway

%install
# Create empty tree
mkdir -p %{buildroot}%{_usrsrc}/%{dkms_name}-%{version}/
cp -fr dkms.conf Makefile src %{buildroot}%{_usrsrc}/%{dkms_name}-%{version}/

%post
dkms add -m %{dkms_name} -v %{version} -q || :
# Rebuild and make available for the currently running kernel
dkms build -m %{dkms_name} -v %{version} -q || :
dkms install -m %{dkms_name} -v %{version} -q --force || :

%preun
# Remove all versions from DKMS registry
dkms remove -m %{dkms_name} -v %{version} -q --all || :

%files
%license LICENSE
%doc README.md
%{_usrsrc}/%{dkms_name}-%{version}

%changelog
* Thu Feb 14 2026 p0358 <p0358@users.noreply.github.com> 0-1
- Initial release.
