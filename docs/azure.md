# Azure demo

The cloud profile loads the temperature and motor simulators and leaves both inactive. Admin routes, the CAN plugin, and a telemetry database are not part of this deployment. History stays in the gateway process. Container stdout is the only log sink. Deleting the resource group ends the showcase.

Build and push the gateway and display images, then deploy `infra/azure/main.bicep` with those image names:

```bash
az deployment group create \
  --resource-group open-gateway \
  --template-file infra/azure/main.bicep \
  --parameters gatewayImage=<registry>/open-gateway:0.1.0 displayImage=<registry>/open-gateway-display:0.1.0
```

The template creates a Container Apps environment, an internal gateway app, and a public display app. The display app listens on port 6080 with `OGW_UI=novnc`. Open `https://<display-fqdn>/vnc.html?autoconnect=1&resize=scale`. Ingress is external only on the display app. The gateway uses `OGW_PROFILE=cloud`. `OGW_MQTT_URL` is empty in this template, so MQTT stays in the catalog and publishes are not sent.

Public ingress serves the Qt window. Scan is not on that ingress. Installing a native plugin stays a local or image build step.

A manual GitHub Actions workflow, `Azure deploy`, is provided for a later credentialed run. It does not run on pull requests. Without `AZURE_CREDENTIALS` it prints the command above and exits successfully so the repository stays usable before an Azure subscription is attached.
