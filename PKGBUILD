pkgname=unlimitedlife
pkgver=1.0.0
pkgrel=1
pkgdesc="My long-term project."
arch=('x86_64')
url="https://github.com/aaaa0ggMC/UnlimitedLife-Linux"
license=('MIT')
depends=('gcc' 'bash' 'glm' 'mesa' 'glew' 'glfw' 'rapidjson')
makedepends=('cmake')
source=("$pkgname-$pkgver.tar.gz::https://github.com/aaaa0ggMC/UnlimitedLife-Linux/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build(){
  cd "$pkgname-$pkgver"
  ./configure
  ./build
}

package(){
  install -d "$pkgdir/usr/bin"
  cp -r CBuild/* "$pkgdir/usr/bin/"
}

post_install(){
  echo "Successfully installed UnlimitedLife"
}


