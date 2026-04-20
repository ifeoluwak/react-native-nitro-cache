require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "NitroCache"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  # Swift C++ interop (CxxStdlib) requires iOS 16+; RN's min_ios_version_supported is often lower.
  s.platforms    = { :ios => '16.0' }
  s.source       = { :git => "https://github.com/ifeoluwak/react-native-nitro-cache.git", :tag => "#{s.version}" }

  s.source_files = [
    "ios/**/*.{swift}",
    "ios/**/*.{m,mm}",
    "cpp/**/*.{hpp,cpp}",
  ]

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'

  load 'nitrogen/generated/ios/NitroCache+autolinking.rb'
  add_nitrogen_files(s)

  install_modules_dependencies(s)
end
