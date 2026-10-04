import SiteClient from "./site-client";
import { getChatGPTUser } from "./chatgpt-auth";

export const dynamic = "force-dynamic";

export default async function Home() {
  const legacyUser = await getChatGPTUser();
  return <SiteClient legacyUser={legacyUser ? { displayName: legacyUser.displayName, email: legacyUser.email } : null} />;
}
