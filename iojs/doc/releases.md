io.js Release Process
=====================

This document describes the technical aspects of the io.js release process. The intended audience is those who have been authorized by the Technical Committee (TC) to create, promote and sign official release builds for io.js, hosted on <https://iojs.org>.

## Who can make a release?

Release authorization is given by the io.js TC. Once authorized, an individual must be have the following:

### 1. Jenkins Release Access

There are three relevant Jenkins jobs that should be used for a release flow:

**a.** **[iojs+any-pr+multi](https://jenkins-iojs.nodesource.com/job/iojs+any-pr+multi/)** is used for a final full-test run to ensure that the current *HEAD* is stable.

**b.** (optional) **[iojs+release+nightly](https://jenkins-iojs.nodesource.com/job/iojs+release+nightly/)** can be used to create a nightly release for the current *HEAD* if public test releases are required. Builds triggered with this job are published straight to <http://iojs.org/download/nightly/> and are available for public download.

**c.** **[iojs+release](https://jenkins-iojs.nodesource.com/job/iojs+release/)** does all of the work to build all required release assets. Promotion of the release files is a manual step once they are ready (see below).

The [io.js build team](https://github.com/nodejs/build) is able to provide this access to individuals authorized by the TC.
 
### 2. <iojs.org> Access

The _dist_ user on iojs.org controls the assets available in <http://iojs.org/download/> (note that <http://iojs.org/dist/> is an alias for <https://iojs.org/download/release/>).

The Jenkins release build slaves upload their artefacts to the web server as the _staging_ user, the _dist_ user has access to move these assets to public access (the _staging_ user does not, for security purposes).

Nightly builds are promoted automatically on the server by a cron task for the _dist_ user.

Release builds require manual promotion by an individual with SSH access to the server as the _dist_ user. The [io.js build team](https://github.com/nodejs/build) is able to provide this access to individuals authorized by the TC.

### 3. A Publicly Listed GPG Key

A SHASUMS256.txt file is produced for every promoted build, nightly and releases. Additionally for releases, this file is signed by the individual responsible for that release. In order to be able to verify downloaded binaries, the public should be able to check that the SHASUMS256.txt file has been signed by someone who has been authorized to create a release.

The GPG keys should be fetchable from a known third-party keyserver, currently the SKS Keyservers at <https://sks-keyservers.net> are recommended. Use the [submission](https://sks-keyservers.net/i/#submit) form to submit a new GPG key. Keys should be fetchable via:

```
gpg --keyserver pool.sks-keyservers.net --recv-keys <FINGERPRINT>
```

Additionally, full GPG key fingerprints for individuals authorized to release should be listed in the io.js GitHub README.md file.

## How to create a release

Notes:

 - Dates listed below as _"YYYY-MM-DD"_ should be the date of the release **as UTC**. Use `date -u +'%Y-%m-%d'` to find out what this is.
 - Version strings are listed below as _"vx.y.z"_, substitute for the release version.

### 1. Ensure that HEAD Is Stable

Run a **[iojs+any-pr+multi](https://jenkins-iojs.nodesource.com/job/iojs+any-pr+multi/)** test run to ensure that the build is stable and the HEAD commit is ready for release.

### 2. Produce a Nightly Build _(optional)_

If there is reason to produce a test release for the purpose of having others try out installers or specifics of builds, produce a nightly build using **[iojs+release+nightly](https://jenkins-iojs.nodesource.com/job/iojs+release+nightly/)** and wait for it to drop in <http://iojs.org/download/nightly/>.

This is particularly recommended if there has been recent work relating to the OS X or Windows installers as they are not tested in any way by CI.

### 4. Update the _CHANGELOG.md_

Use the following git command to produce a list of commits since the last release:

```
git log --pretty=format:"* [%h] - %s (%aN)" \
  --since="$(git show -s --format=%ad `git rev-list --tags --max-count=1`)"
```

_(You will probably need to omit the first two commits as they relate to the last release.)_

The _CHANGELOG.md_ entry should take the following form:

```
## YYYY-MM-DD, Version x.y.z, @user

### Notable changes

* List interesting changes here
* Particularly changes that are responsible for minor or major version bumps
* Also be sure to look at any changes introduced by dependencies such as npm
* ... and include any notable items from there

### Known issues

* Include this section if there are any known problems with this release
* Scan GitHub for unresolved problems that users may need to be aware of

### Commits

* Include the full list of commits since the last release here
```

### 5. Update _src/node_version.h_

The following macros should already be set for the release since they will have been updated directly following the last release. They shouldn't require changing:

```
#define NODE_MAJOR_VERSION x
#define NODE_MINOR_VERSION y
#define NODE_PATCH_VERSION z
```

However, the `NODE_VERSION_IS_RELEASE` macro needs to be set to `1` for the build to be produced with a version string that does not have a trailing pre-release tag:

```
#define NODE_VERSION_IS_RELEASE 1
```

**Also consider whether to bump `NODE_MODULE_VERSION`**:

This macro is used to signal an ABI version for native addons. It currently has two common uses in the community:

* Determining what API to work against for compiling native addons, e.g. [NAN](https://github.com/rvagg/nan) uses it to form a compatibility-layer for much of what it wraps.
* Determining the ABI for downloading pre-built binaries of native addons, e.g. [node-pre-gyp](https://github.com/mapbox/node-pre-gyp) uses this value as exposed via `process.versions.modules` to help determine the appropriate binary to download at install-time.

The general rule is to bump this version when there are _breaking ABI_ changes and also if there are non-trivial API changes. The rules are not yet strictly defined, so if in doubt, please confer with someone that will have a more informed perspective, such as a member of the NAN team.

### 6. Create Release Commit

The _CHANGELOG.md_ and _src/node_version.h_ changes should be the final commit that will be tagged for the release.

When committing these to git, use the following message format:

```
YYYY-MM-DD io.js vx.y.z Release

Notable changes:

* Copy the notable changes list here
```

### 7. Tag and Sign the Release Commit

Tag the release as <b><code>vx.y.z</code></b> and sign **using the same GPG key that will be used to sign SHASUMS256.txt**.

```
git tag -sm 'YYYY-MM-DD io.js vz.y.x Release' vx.y.z
```

### 8. Set Up For the Next Release

Edit _src/node_version.h_ again and:

* Increment `NODE_PATCH_VERSION` by one
* Change `NODE_VERSION_IS_RELEASE` back to `0`

Commit this change with:

```
git commit -am 'Working on vx.y.z' # where 'z' is the incremented patch number
```

This sets up the branch so that nightly builds are produced with the next version number _and_ a pre-release tag.

### 9. Push to GitHub

Push the changes along with the tag you created:

```
git push origin branch vx.y.z
# where "branch" is the working branch and "vx.y.z" the the release version
```

### 9. Produce Release Builds

Use **[iojs+release](https://jenkins-iojs.nodesource.com/job/iojs+release/)** to produce release artefacts. Enter the "vx.y.z" version string for this release and it will fetch your tagged commit.

Artefacts from each slave are uploaded to Jenkins and are available if further testing is required. Use this opportunity particularly to test OS X and Windows installers if there are any concerns. Click through to the individual slaves for a run to find the artefacts. For example, the Windows 64-bit .msi file for v1.0.4 can be found [here](https://jenkins-iojs.nodesource.com/job/iojs+release/20/nodes=iojs-win2008r2-release-x64/).

All release slaves should achieve "SUCCESS" (and be blue, not red). A release with failures should not be promoted, there are likely problems to be investigated.

Note that you do not have to wait for the ARM builds if they are take longer than the others. It is only necessary to have the main Linux (x64 and x86), OS X .pkg and .tar.gz, Windows (x64 and x86) .msi and .exe, source and docs (both produced currently by an OS X slave). i.e. the slaves with "arm" in their name don't need to have finished to progress to the next step. However, **if you promote builds _before_ ARM builds have finished, you must repeat the promotion step for the ARM builds when they are ready**.

### 10. Promote and Sign the Release Builds

**It is important that the same individual who signed the release tag be the one to promote the builds as the SHASUMS256.txt file needs to be signed with the same GPG key!**

When you are confident that the build slaves have properly produced usable artefacts and uploaded them to the web server you can promote them to release status. This is done by interacting with the web server via the _dist_ user.

The _tools/release.sh_ script may be used to promote and sign the build. When run, it will perform the following actions:

**a.** Select a GPG key from your private keys, it will use a command similar to: `gpg --list-secret-keys` to list your keys. If you don't have any keys, it will bail (why are you releasing? Your tag should be signed!). If you have only one key, it will use that. If you have more than one key it will ask you to select one from the list. Be sure to use the same key that you signed your git tag with.

**b.** Log in to the server via SSH and check for releases that can be promoted, along with the list of artefacts. It will use the `dist-promotable` command on the server to find these. You will be asked, for each promotable release, whether you want to proceed. If there is more than one release to promote (there shouldn't be), be sure to only promote the release you are responsible for.

**c.** Log in to the server via SSH and run the promote script for the given release. The command on the server will be similar to: `dist-promote vx.y.z`. After this step, the release artefacts will be available for download and a SHASUMS256.txt file will be present. The release will still be unsigned, however.

**d.** Download SHASUMS256.txt to your computer using SCP into a temporary directory.

**e.** Sign the SHASUMS256.txt file using a command similar to: `gpg --default-key YOURKEY --clearsign /path/to/SHASUMS256.txt`. You will be prompted by GPG for your password for this to work. The signed file will be named SHASUMS256.txt.asc.

**f.** Output an ASCII armored version of your public GPG key, using a command similar to: `gpg --default-key YOURKEY --armor --export --output /path/to/SHASUMS256.txt.gpg`. This does not require your password and is mainly a convenience for users although not the recommended way to get a copy of your key.

**g.** Upload the SHASUMS256.txt\* files back to the server into the release directory.

If you didn't wait for ARM builds in the previous step before promoting the release, you should re-run _tools/release.sh_ after the ARM builds have finished and it will move the ARM artefacts into the correct location and you will be prompted to re-sign SHASUMS256.txt.

### 11. Check the Release

Your release should be available at <https://iojs.org/dist/vx.y.z/> and also <https://iojs.org/dist/latest/>. Check that the appropriate files are in place, you may also want to check that the binaries are working as appropriate and have the right internal version strings. Check that the API docs are available at <https://iojs.org/api/>. Check that the release catalog files are correct at <https://iojs.org/dist/index.tab> and <https://iojs.org/dist/index.json>.

### 12. Announce

_TODO: Update doc with announce procedure when we figure this out._

### 13. Celebrate

_In whatever form you do this..._
