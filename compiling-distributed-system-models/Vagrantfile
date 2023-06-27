# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "bento/ubuntu-20.04"

  config.vm.network "private_network", ip: "192.168.56.0"

  config.vm.provider "virtualbox" do |v|
    v.memory = 4096
  end

  config.vm.provision "shell", inline: <<-SHELL
    cp -r /vagrant/image ./image
    if ! grep -q azurebench ./.ssh/authorized_keys; then
      echo copying public key...
      cat /vagrant/id_rsa.pub >>./.ssh/authorized_keys
    else
      echo public key already installed.
    fi
    chown -R vagrant .
    cd image
    runuser -u vagrant -- bash ./provision.sh
  SHELL
end
