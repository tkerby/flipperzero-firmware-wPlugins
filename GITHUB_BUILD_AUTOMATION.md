# How to Automate Github Build for RogueMaster Flipper
This describes how you can automate a custom build process to generate your own releases on Github. This may be useful to include custom assets like @tkerby has used for UK based RF hopping.

## Step 1: Fork the repository
Fork the (RogueMaster Flipper Repository)[https://github.com/RogueMaster/flipperzero-firmware-wPlugins] by clicking the "Fork" button in the top right of the Github web interface. This places a fork in your own workspace.

## Step 2: Automate the build using Github Workflows
Create a file "/.github/workflows/build.yml" using the "Add File" button and copy this contents into it
```
name: build

on:
  push:
    branches: [ 420 ]
  pull_request:
    branches: [ 420 ]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - name: checkout
        uses: actions/checkout@v2

      - name: build updater package
        run: ./fbt updater_package

      - name: zip updater package
        run: cd dist/f7-C/f7-update-RM420FAP && zip ${{ github.sha }}.zip *

      - name: upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: ${{ github.sha }}
          path: dist/f7-C/f7-update-RM420FAP/${{ github.sha }}.zip
          
      - name: prerelease
        uses: "marvinpinto/action-automatic-releases@latest"
        with:
          repo_token: "${{ secrets.GITHUB_TOKEN }}"
          automatic_release_tag: "latest"
          prerelease: true
          title: "Latest Build"
          files: dist/f7-C/f7-update-RM420FAP/${{ github.sha }}.zip
          
```
## Step 3: Change any files
You might want to change assets including animations or the resources installed with the build. Resources can be found in "/assets/resources/" and animations in "/assets/dolphin". Commit your changes.

## Step 4: Install your custom release
After about 5 minutes your build should complete (you can check the "Actions" tab) and you'll see a release tagged latest available for you to install

## Step 5: Keep up to date
When the upstream repository is updated, you can see the "Sync Fork" button. FOllow the instructions there to sync the latest upstream changes and automatically generate a new release. Your changes will persist and any conflicts identified.

