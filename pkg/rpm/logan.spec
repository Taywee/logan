Name:           logan
Version:        1.0
Release:        0%{?dist}
Summary:        log frequency analyzer

Group:          Applications/Productivity
License:        GPLv3+
URL:            https://github.com/Taywee/%{name}
Source:         https://github.com/Taywee/%{name}/archive/%{version}.tar.gz
BuildRequires:  gcc

%description
Processes and provides frequency analysis of log files.  Optionally provides a
specific matching percentage for approximate message matching, with hashing
optimization for exact matching.

%prep
%setup -q

%build
make CXX=g++ %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}/usr

%files
%doc README.md LICENSE
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1.gz

%changelog

