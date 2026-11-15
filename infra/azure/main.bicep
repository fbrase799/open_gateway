param location string = resourceGroup().location
param gatewayImage string
param displayImage string
param namePrefix string = 'ogw'

resource logs 'Microsoft.OperationalInsights/workspaces@2023-09-01' = {
  name: '${namePrefix}-logs'
  location: location
  properties: {
    sku: {
      name: 'PerGB2018'
    }
    retentionInDays: 30
  }
}

resource environment 'Microsoft.App/managedEnvironments@2024-03-01' = {
  name: '${namePrefix}-env'
  location: location
  properties: {
    appLogsConfiguration: {
      destination: 'log-analytics'
      logAnalyticsConfiguration: {
        customerId: logs.properties.customerId
        sharedKey: logs.listKeys().primarySharedKey
      }
    }
  }
}

resource gateway 'Microsoft.App/containerApps@2024-03-01' = {
  name: '${namePrefix}-gateway'
  location: location
  properties: {
    managedEnvironmentId: environment.id
    configuration: {
      ingress: {
        external: false
        targetPort: 8080
        transport: 'http'
      }
    }
    template: {
      containers: [
        {
          name: 'gateway'
          image: gatewayImage
          env: [
            { name: 'OGW_PROFILE', value: 'cloud' }
            { name: 'OGW_ADMIN_API', value: 'false' }
          ]
          resources: {
            cpu: json('0.5')
            memory: '1Gi'
          }
        }
      ]
    }
  }
}

resource display 'Microsoft.App/containerApps@2024-03-01' = {
  name: '${namePrefix}-display'
  location: location
  properties: {
    managedEnvironmentId: environment.id
    configuration: {
      ingress: {
        external: true
        targetPort: 6080
        transport: 'auto'
      }
    }
    template: {
      containers: [
        {
          name: 'display'
          image: displayImage
          env: [
            { name: 'OGW_UI', value: 'novnc' }
            { name: 'OGW_API', value: 'http://${namePrefix}-gateway' }
            { name: 'OGW_MQTT_URL', value: '' }
          ]
          resources: {
            cpu: json('0.5')
            memory: '1Gi'
          }
        }
      ]
    }
  }
}

output displayFqdn string = display.properties.configuration.ingress.fqdn
