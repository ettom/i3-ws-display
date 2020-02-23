pkgname=i3-ws-display-git
_pkgname=ws-display
pkgver=0.0.1
pkgrel=1
pkgdesc="Display active i3 workspaces on a 7-segment display"
arch=('x86_64')
url="https://github.com/ettom/i3-ws-display"
license=('GPL3')
depends=('i3-wm' 'libserial' 'libsigc++' 'jsoncpp')
makedepends=('cmake' 'git')
source=("${_pkgname}::git+${url}.git")
md5sums=("SKIP")

prepare() {
	mkdir -p "${_pkgname}/build"
}

build() {
	cd "${_pkgname}/build" || exit 1
	cmake -DCMAKE_INSTALL_PREFIX=/usr ..
	cmake --build .
}

package() {
	cmake --build "${_pkgname}/build" --target install -- DESTDIR="${pkgdir}"
}