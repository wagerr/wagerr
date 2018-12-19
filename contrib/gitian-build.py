#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys

def setup():
    global args, workdir
    programs = ['ruby', 'git', 'apt-cacher-ng', 'make', 'wget']
    if args.kvm:
        programs += ['python-vm-builder', 'qemu-kvm', 'qemu-utils']
    elif args.docker:
        dockers = ['docker.io', 'docker-ce']
        for i in dockers:
            return_code = subprocess.call(['sudo', 'apt-get', 'install', '-qq', i])
            if return_code == 0:
                break
        if return_code != 0:
            print('Cannot find any way to install docker', file=sys.stderr)
            exit(1)
    else:
        programs += ['lxc', 'debootstrap']
    subprocess.check_call(['sudo', 'apt-get', 'install', '-qq'] + programs)
    if not os.path.isdir('gitian.sigs'):
        subprocess.check_call(['git', 'clone', 'https://github.com/wagerr/gitian.sigs.git', 'gitian.sigs'])
    if not os.path.isdir('wagerr-detached-sigs'):
        subprocess.check_call(['git', 'clone', 'https://github.com/wagerr/wagerr-detached-sigs.git'])
    if not os.path.isdir('gitian-builder'):
        subprocess.check_call(['git', 'clone', 'https://github.com/devrandom/gitian-builder.git'])
    if not os.path.isdir('wagerr'):
        subprocess.check_call(['git', 'clone', 'https://github.com/wagerr/wagerr.git'])
    os.chdir('gitian-builder')
    make_image_prog = ['bin/make-base-vm', '--suite', 'trusty', '--arch', 'amd64']
    if args.docker:
        make_image_prog += ['--docker']
    elif not args.kvm:
        make_image_prog += ['--lxc']
    subprocess.check_call(make_image_prog)
    os.chdir(workdir)
    if args.is_trusty and not args.kvm and not args.docker:
        subprocess.check_call(['sudo', 'sed', '-i', 's/lxcbr0/br0/', '/etc/default/lxc-net'])
        print('Reboot is required')
        exit(0)

def build():
    global args, workdir

    os.makedirs('wagerr-binaries/' + args.version, exist_ok=True)
    print('\nBuilding Dependencies\n')
    os.chdir('gitian-builder')
    os.makedirs('inputs', exist_ok=True)

    subprocess.check_call(['wget', '-O', 'inputs/osslsigncode-1.7.1.tar.gz', '-N', '-P', 'inputs', 'https://github.com/cevap/osslsigncode/archive/v1.7.1.tar.gz'])
    subprocess.check_call(['wget', '-O', 'inputs/osslsigncode-Backports-to-1.7.1.patch', '-N', '-P', 'inputs', 'https://github.com/cevap/osslsigncode/releases/download/v1.7.1/osslsigncode-Backports-to-1.7.1.patch'])
    subprocess.check_call(['make', '-C', '../wagerr/depends', 'download', 'SOURCES_PATH=' + os.getcwd() + '/cache/common'])

    if args.linux:
        print('\nCompiling ' + args.version + ' Linux')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'wagerr='+args.commit, '--url', 'wagerr='+args.url, '../wagerr/contrib/gitian-descriptors/gitian-linux.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-linux', '--destination', '../gitian.sigs/', '../wagerr/contrib/gitian-descriptors/gitian-linux.yml'])
        subprocess.check_call('mv build/out/wagerr-*.tar.gz build/out/src/wagerr-*.tar.gz ../wagerr-binaries/'+args.version, shell=True)

    if args.windows:
        print('\nCompiling ' + args.version + ' Windows')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'wagerr='+args.commit, '--url', 'wagerr='+args.url, '../wagerr/contrib/gitian-descriptors/gitian-win.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-unsigned', '--destination', '../gitian.sigs/', '../wagerr/contrib/gitian-descriptors/gitian-win.yml'])
        subprocess.check_call('mv build/out/wagerr-*-win-unsigned.tar.gz inputs/', shell=True)
        subprocess.check_call('mv build/out/wagerr-*.zip build/out/wagerr-*.exe ../wagerr-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nCompiling ' + args.version + ' MacOS')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'wagerr='+args.commit, '--url', 'wagerr='+args.url, '../wagerr/contrib/gitian-descriptors/gitian-osx.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-unsigned', '--destination', '../gitian.sigs/', '../wagerr/contrib/gitian-descriptors/gitian-osx.yml'])
        subprocess.check_call('mv build/out/wagerr-*-osx-unsigned.tar.gz inputs/', shell=True)
        subprocess.check_call('mv build/out/wagerr-*.tar.gz build/out/wagerr-*.dmg ../wagerr-binaries/'+args.version, shell=True)

    os.chdir(workdir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Unsigned Sigs\n')
        os.chdir('gitian.sigs')
        subprocess.check_call(['git', 'config', 'user.signingkey', args.signer])
        if args.linux:
            subprocess.check_call(['git', 'add', args.version+'-linux/'+args.signer])
        if args.windows:
            subprocess.check_call(['git', 'add', args.version+'-win-unsigned/'+args.signer])
        if args.macos:
            subprocess.check_call(['git', 'add', args.version+'-osx-unsigned/'+args.signer])
        subprocess.check_call(['git', 'commit', '-m', 'Add '+args.version+' unsigned sigs for '+args.signer])
        os.chdir(workdir)

def sign():
    global args, workdir
    os.chdir('gitian-builder')

    if args.windows:
        print('\nSigning ' + args.version + ' Windows')
        subprocess.check_call('cp inputs/wagerr-' + args.version + '-win-unsigned.tar.gz inputs/wagerr-win-unsigned.tar.gz', shell=True)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature='+args.commit, '../wagerr/contrib/gitian-descriptors/gitian-win-signer.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-signed', '--destination', '../gitian.sigs/', '../wagerr/contrib/gitian-descriptors/gitian-win-signer.yml'])
        subprocess.check_call('mv build/out/wagerr-*win64-setup.exe ../wagerr-binaries/'+args.version, shell=True)
        subprocess.check_call('mv build/out/wagerr-*win32-setup.exe ../wagerr-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nSigning ' + args.version + ' MacOS')
        subprocess.check_call('cp inputs/wagerr-' + args.version + '-osx-unsigned.tar.gz inputs/wagerr-osx-unsigned.tar.gz', shell=True)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature='+args.commit, '../wagerr/contrib/gitian-descriptors/gitian-osx-signer.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-signed', '--destination', '../gitian.sigs/', '../wagerr/contrib/gitian-descriptors/gitian-osx-signer.yml'])
        subprocess.check_call('mv build/out/wagerr-osx-signed.dmg ../wagerr-binaries/'+args.version+'/wagerr-'+args.version+'-osx.dmg', shell=True)

    os.chdir(workdir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Signed Sigs\n')
        os.chdir('gitian.sigs')

        if args.windows:
            subprocess.check_call(['git', 'add', args.version+'-win-signed/'+args.signer])
        if args.macos:
            subprocess.check_call(['git', 'add', args.version+'-osx-signed/'+args.signer])

        subprocess.check_call(['git', 'commit', '-S', '-m', 'Add '+args.version+' signed binary sigs for '+args.signer])
        os.chdir(workdir)

def verify():
    global args, workdir
    os.chdir('gitian-builder')

    if args.linux:
        print('\nVerifying v'+args.version+' Linux\n')
        subprocess.check_call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-linux', '../wagerr/contrib/gitian-descriptors/gitian-linux.yml'])
        print('\nVerifying v'+args.version+' Linux\n')
        subprocess.check_call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-linux', '../wagerr/contrib/gitian-descriptors/gitian-linux.yml'])

    if args.windows:
        print('\nVerifying v'+args.version+' Windows\n')
        subprocess.check_call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-win-unsigned', '../wagerr/contrib/gitian-descriptors/gitian-win.yml'])
        if args.sign:
            print('\nVerifying v'+args.version+' Signed Windows\n')
            subprocess.check_call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-win-signed', '../wagerr/contrib/gitian-descriptors/gitian-win-signer.yml'])

    if args.macos:
        print('\nVerifying v'+args.version+' MacOS\n')
        subprocess.check_call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-osx-unsigned', '../wagerr/contrib/gitian-descriptors/gitian-osx.yml'])
        if args.sign:
            print('\nVerifying v'+args.version+' Signed MacOS\n')
            subprocess.check_call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-osx-signed', '../wagerr/contrib/gitian-descriptors/gitian-osx-signer.yml'])

    os.chdir(workdir)

def main():
    global args, workdir

    parser = argparse.ArgumentParser(usage='%(prog)s [options] signer version')
    parser.add_argument('-c', '--commit', action='store_true', dest='commit', help='Indicate that the version argument is for a commit or branch')
    parser.add_argument('-p', '--pull', action='store_true', dest='pull', help='Indicate that the version argument is the number of a github repository pull request')
    parser.add_argument('-u', '--url', dest='url', default='https://github.com/wagerr/wagerr', help='Specify the URL of the repository. Default is %(default)s')
    parser.add_argument('-v', '--verify', action='store_true', dest='verify', help='Verify the Gitian build')
    parser.add_argument('-b', '--build', action='store_true', dest='build', help='Do a Gitian build')
    parser.add_argument('-s', '--sign', action='store_true', dest='sign', help='Make signed binaries for Windows and MacOS')
    parser.add_argument('-B', '--buildsign', action='store_true', dest='buildsign', help='Build both signed and unsigned binaries')
    parser.add_argument('-o', '--os', dest='os', default='lwm', help='Specify which Operating Systems the build is for. Default is %(default)s. l for Linux, w for Windows, m for MacOS')
    parser.add_argument('-j', '--jobs', dest='jobs', default='2', help='Number of processes to use. Default %(default)s')
    parser.add_argument('-m', '--memory', dest='memory', default='2000', help='Memory to allocate in MiB. Default %(default)s')
    parser.add_argument('-k', '--kvm', action='store_true', dest='kvm', help='Use KVM instead of LXC')
    parser.add_argument('-d', '--docker', action='store_true', dest='docker', help='Use Docker instead of LXC')
    parser.add_argument('-S', '--setup', action='store_true', dest='setup', help='Set up the Gitian building environment. Uses LXC. If you want to use KVM, use the --kvm option. Only works on Debian-based systems (Ubuntu, Debian)')
    parser.add_argument('-D', '--detach-sign', action='store_true', dest='detach_sign', help='Create the assert file for detached signing. Will not commit anything.')
    parser.add_argument('-n', '--no-commit', action='store_false', dest='commit_files', help='Do not commit anything to git')
    parser.add_argument('signer', help='GPG signer to sign each build assert file')
    parser.add_argument('version', help='Version number, commit, or branch to build. If building a commit or branch, the -c option must be specified')

    args = parser.parse_args()
    workdir = os.getcwd()

    args.linux = 'l' in args.os
    args.windows = 'w' in args.os
    args.macos = 'm' in args.os

    args.is_trusty = b'trusty' in subprocess.check_output(['lsb_release', '-cs'])

    if args.buildsign:
        args.build=True
        args.sign=True

    if args.kvm and args.docker:
        raise Exception('Error: cannot have both kvm and docker')

    args.sign_prog = 'true' if args.detach_sign else 'gpg --detach-sign'

    # Set environment variable USE_LXC or USE_DOCKER, let gitian-builder know that we use lxc or docker
    if args.docker:
        os.environ['USE_DOCKER'] = '1'
    elif not args.kvm:
        os.environ['USE_LXC'] = '1'
        if not 'GITIAN_HOST_IP' in os.environ.keys():
            os.environ['GITIAN_HOST_IP'] = '10.0.3.1'
        if not 'LXC_GUEST_IP' in os.environ.keys():
            os.environ['LXC_GUEST_IP'] = '10.0.3.5'

    # Disable for MacOS if no SDK found
    if args.macos and not os.path.isfile('gitian-builder/inputs/MacOSX10.11.sdk.tar.xz'):
    	subprocess.check_call(['wget', '-O', 'gitian-builder/inputs/MacOSX10.11.sdk.tar.xz', '-N', '-P', 'inputs', 'https://github.com/gitianuser/MacOSX-SDKs/releases/download/MacOSX10.11.sdk/MacOSX10.11.sdk.tar.xz'])
    	if args.macos and not os.path.isfile('gitian-builder/inputs/MacOSX10.11.sdk.tar.xz'):
        	print('Cannot build for MacOS, SDK does not exist. Will build for other OSes')
        	args.macos = False

    script_name = os.path.basename(sys.argv[0])
    # Signer and version shouldn't be empty
    if args.signer == '':
        print(script_name+': Missing signer.')
        print('Try '+script_name+' --help for more information')
        exit(1)
    if args.version == '':
        print(script_name+': Missing version.')
        print('Try '+script_name+' --help for more information')
        exit(1)

    # Add leading 'v' for tags
    if args.commit and args.pull:
        raise Exception('Cannot have both commit and pull')
    args.commit = ('' if args.commit else 'v') + args.version

    if args.setup:
        setup()

    os.chdir('wagerr')
    if args.pull:
        subprocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
        os.chdir('../gitian-builder/inputs/wagerr')
        subprocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
        args.commit = subprocess.check_output(['git', 'show', '-s', '--format=%H', 'FETCH_HEAD'], universal_newlines=True, encoding='utf8').strip()
        args.version = 'pull-' + args.version
    print(args.commit)
    subprocess.check_call(['git', 'fetch'])
    subprocess.check_call(['git', 'checkout', args.commit])
    os.chdir(workdir)

    if args.build:
        build()

    if args.sign:
        sign()

    if args.verify:
        verify()

if __name__ == '__main__':
    main()
