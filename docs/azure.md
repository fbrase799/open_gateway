# Azure demo

The cloud profile starts the temperature simulator and leaves the motor simulator inactive so the sample client can enable it. Admin routes, the CAN plugin, the Qt display, and a telemetry database are not part of this deployment. History stays in the gateway process. Container stdout is the only log sink. Deleting the resource group ends the showcase.

Build and push the gateway and web images, then deploy `infra/azure/main.bicep` with those image names:

```bash
az deployment group create \
  --resource-group open-gateway \
  --template-file infra/azure/main.bicep \
  --parameters gatewayImage=<registry>/open-gateway:0.1.0 webImage=<registry>/open-gateway-web:0.1.0
```

The template creates a Container Apps environment, an internal gateway app, and a public web app. Ingress is external only on the web app. Set `OGW_PROFILE=cloud` on the gateway, which the image entrypoint already honors when that variable is present. The Bicep file sets it.

Public ingress serves `sample_client` and proxies support GET, support PUT, and telemetry. Scan is not on that ingress. Installing a native plugin stays a local or image build step.

A manual GitHub Actions workflow, `Azure deploy`, is provided for a later credentialed run. It does not run on pull requests. Without `AZURE_CREDENTIALS` it prints the command above and exits successfully so the repository stays usable before an Azure subscription is attached.
