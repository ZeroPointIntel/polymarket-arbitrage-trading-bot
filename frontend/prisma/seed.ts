import { PrismaClient } from '@prisma/client'
import bcrypt from 'bcryptjs'

const prisma = new PrismaClient()

async function main() {
  console.log(`Start seeding ...`)
  
  // NOTE: If you change the super user credentials here, make sure to update
  // the documentation in /README.md under the Authentication section.

  const superEmail = 'admin@tradebot.local'
  const superPassword = 'SuperSecretPassword123!'

  const hashedPassword = await bcrypt.hash(superPassword, 10)

  const user = await prisma.user.upsert({
    where: { email: superEmail },
    update: {
      password: hashedPassword,
    },
    create: {
      email: superEmail,
      password: hashedPassword,
    },
  })

  console.log(`Created super user with email: ${user.email}`)
  console.log(`Seeding finished.`)
}

main()
  .then(async () => {
    await prisma.$disconnect()
  })
  .catch(async (e) => {
    console.error(e)
    await prisma.$disconnect()
    process.exit(1)
  })
